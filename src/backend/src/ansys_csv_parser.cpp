#include "ansys_csv_parser.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <limits>
#include <cmath>

namespace Workbench {

static std::string trimStr(const std::string& str) {
    auto start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

static std::vector<std::string> splitCSVLine(const std::string& line) {
    std::vector<std::string> tokens;
    std::string token;
    bool inQuotes = false;
    for (char c : line) {
        if (c == '"') {
            inQuotes = !inQuotes;
        } else if (c == ',' && !inQuotes) {
            tokens.push_back(trimStr(token));
            token.clear();
        } else {
            token += c;
        }
    }
    tokens.push_back(trimStr(token));
    return tokens;
}

static std::string toLowerStr(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return str;
}

ANSYSResultSet ANSYSCSVParser::parseCSV(const std::string& filePath) {
    ANSYSResultSet result;
    std::ifstream file(filePath, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        result.isValid = false;
        result.errorMessage = "Failed to open CSV file: " + filePath + " (File not found or access denied)";
        return result;
    }

    file.seekg(0, std::ios::end);
    auto fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    if (fileSize == 0) {
        result.isValid = false;
        result.errorMessage = "Empty file: File size is 0 bytes. Expected header 'node_id,x_m,y_m,z_m,temperature_C'.";
        return result;
    }

    constexpr size_t MAX_TESTED_SIZE = 100 * 1024 * 1024;
    if (static_cast<size_t>(fileSize) > MAX_TESTED_SIZE) {
        result.isValid = false;
        result.errorMessage = "CSV file exceeds maximum tested size limit of 100 MB (" +
                              std::to_string(fileSize / (1024 * 1024)) + " MB).";
        return result;
    }

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return parseCSVFromString(content);
}

ANSYSResultSet ANSYSCSVParser::parseCSVFromString(const std::string& content) {
    ANSYSResultSet result;
    if (trimStr(content).empty()) {
        result.isValid = false;
        result.errorMessage = "Empty file: CSV content is empty.";
        return result;
    }

    std::istringstream stream(content);
    std::string line;
    size_t lineNum = 0;

    int idxNode = -1, idxX = -1, idxY = -1, idxZ = -1, idxTemp = -1;

    // Find and parse header
    while (std::getline(stream, line)) {
        lineNum++;
        std::string trimmed = trimStr(line);
        if (trimmed.empty()) continue;

        // Check for comment line
        if (trimmed[0] == '#') {
            if (trimmed.find("provenance") != std::string::npos ||
                trimmed.find("Provenance") != std::string::npos) {
                result.provenance = trimStr(trimmed.substr(1));
            }
            continue;
        }

        auto tokens = splitCSVLine(trimmed);
        for (size_t i = 0; i < tokens.size(); ++i) {
            std::string lower = toLowerStr(tokens[i]);
            if (lower == "node_id" || lower == "nodeid" || lower == "node") idxNode = static_cast<int>(i);
            else if (lower == "x_m" || lower == "x [m]" || lower == "x") idxX = static_cast<int>(i);
            else if (lower == "y_m" || lower == "y [m]" || lower == "y") idxY = static_cast<int>(i);
            else if (lower == "z_m" || lower == "z [m]" || lower == "z") idxZ = static_cast<int>(i);
            else if (lower == "temperature_c" || lower == "temperature [c]" || lower == "temperature" || lower == "temp_c") idxTemp = static_cast<int>(i);
        }
        break;
    }

    if (idxNode == -1 || idxX == -1 || idxY == -1 || idxZ == -1 || idxTemp == -1) {
        result.isValid = false;
        result.errorMessage = "Line " + std::to_string(lineNum) +
                              ": Invalid CSV schema. Required columns: node_id,x_m,y_m,z_m,temperature_C.";
        return result;
    }

    float minX = std::numeric_limits<float>::max();
    float minY = std::numeric_limits<float>::max();
    float minZ = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::lowest();
    float maxY = std::numeric_limits<float>::lowest();
    float maxZ = std::numeric_limits<float>::lowest();
    float minT = std::numeric_limits<float>::max();
    float maxT = std::numeric_limits<float>::lowest();
    double sumT = 0.0;

    while (std::getline(stream, line)) {
        lineNum++;
        std::string trimmed = trimStr(line);
        if (trimmed.empty() || trimmed[0] == '#') continue;

        auto tokens = splitCSVLine(trimmed);
        int maxRequiredIdx = std::max({idxNode, idxX, idxY, idxZ, idxTemp});
        if (static_cast<int>(tokens.size()) <= maxRequiredIdx) {
            result.isValid = false;
            result.errorMessage = "Line " + std::to_string(lineNum) +
                                  ": Insufficient columns (expected at least " +
                                  std::to_string(maxRequiredIdx + 1) + ", found " +
                                  std::to_string(tokens.size()) + ").";
            return result;
        }

        ANSYSNode node;
        try {
            node.nodeId = std::stoi(tokens[idxNode]);
            node.x = std::stof(tokens[idxX]);
            node.y = std::stof(tokens[idxY]);
            node.z = std::stof(tokens[idxZ]);
            node.temperatureC = std::stof(tokens[idxTemp]);
        } catch (const std::exception& e) {
            result.isValid = false;
            result.errorMessage = "Line " + std::to_string(lineNum) +
                                  ": Malformed numeric field or parsing error: " + e.what();
            return result;
        }

        if (!std::isfinite(node.x) || !std::isfinite(node.y) ||
            !std::isfinite(node.z) || !std::isfinite(node.temperatureC)) {
            result.isValid = false;
            result.errorMessage = "Line " + std::to_string(lineNum) +
                                  ": Non-finite value detected (NaN or Inf).";
            return result;
        }

        if (node.x < minX) minX = node.x;
        if (node.y < minY) minY = node.y;
        if (node.z < minZ) minZ = node.z;
        if (node.x > maxX) maxX = node.x;
        if (node.y > maxY) maxY = node.y;
        if (node.z > maxZ) maxZ = node.z;

        if (node.temperatureC < minT) minT = node.temperatureC;
        if (node.temperatureC > maxT) maxT = node.temperatureC;
        sumT += node.temperatureC;

        result.nodes.push_back(node);
    }

    if (result.nodes.empty()) {
        result.isValid = false;
        result.errorMessage = "CSV file contains header but no data rows.";
        return result;
    }

    result.nodeCount = result.nodes.size();
    result.bboxMin = {minX, minY, minZ};
    result.bboxMax = {maxX, maxY, maxZ};
    result.center = {0.5f * (minX + maxX), 0.5f * (minY + maxY), 0.5f * (minZ + maxZ)};
    result.minTemperature = minT;
    result.maxTemperature = maxT;
    result.avgTemperature = static_cast<float>(sumT / result.nodeCount);
    result.isValid = true;

    return result;
}

std::string ANSYSResultSet::toJson() const {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(5);
    ss << "{\n";
    ss << "  \"valid\": " << (isValid ? "true" : "false") << ",\n";
    ss << "  \"error\": \"" << errorMessage << "\",\n";
    ss << "  \"provenance\": \"" << provenance << "\",\n";
    ss << "  \"nodeCount\": " << nodeCount << ",\n";
    ss << "  \"temperatureRange\": {\n";
    ss << "    \"min\": " << minTemperature << ",\n";
    ss << "    \"max\": " << maxTemperature << ",\n";
    ss << "    \"avg\": " << avgTemperature << ",\n";
    ss << "    \"unit\": \"degC\"\n";
    ss << "  },\n";
    ss << "  \"bounds\": {\n";
    ss << "    \"min\": [" << bboxMin.x << ", " << bboxMin.y << ", " << bboxMin.z << "],\n";
    ss << "    \"max\": [" << bboxMax.x << ", " << bboxMax.y << ", " << bboxMax.z << "],\n";
    ss << "    \"center\": [" << center.x << ", " << center.y << ", " << center.z << "],\n";
    ss << "    \"unit\": \"m\"\n";
    ss << "  },\n";
    ss << "  \"nodes\": [\n";

    for (size_t i = 0; i < nodes.size(); ++i) {
        const auto& n = nodes[i];
        ss << "    {"
           << "\"id\":" << n.nodeId << ","
           << "\"x\":" << n.x << ","
           << "\"y\":" << n.y << ","
           << "\"z\":" << n.z << ","
           << "\"t\":" << n.temperatureC << "}";
        if (i + 1 < nodes.size()) ss << ",";
        ss << "\n";
    }
    ss << "  ]\n";
    ss << "}";
    return ss.str();
}

} // namespace Workbench
