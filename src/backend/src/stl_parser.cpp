#include "stl_parser.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <iomanip>

namespace Workbench {

static std::string trim(const std::string& str) {
    auto start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

static std::string toLower(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return str;
}

STLMesh STLParser::parseAscii(const std::string& filePath) {
    STLMesh mesh;
    std::ifstream file(filePath, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        mesh.isValid = false;
        mesh.errorMessage = "Failed to open file: " + filePath + " (File not found or access denied)";
        return mesh;
    }

    file.seekg(0, std::ios::end);
    auto fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    if (fileSize == 0) {
        mesh.isValid = false;
        mesh.errorMessage = "Empty file: File size is 0 bytes. Valid ASCII STL files must contain solid definitions.";
        return mesh;
    }

    constexpr size_t MAX_TESTED_SIZE = 50 * 1024 * 1024;
    if (static_cast<size_t>(fileSize) > MAX_TESTED_SIZE) {
        mesh.isValid = false;
        mesh.errorMessage = "File exceeds maximum tested size limit of 50 MB (current size: " +
                            std::to_string(fileSize / (1024 * 1024)) + " MB).";
        return mesh;
    }

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return parseAsciiFromString(content);
}

STLMesh STLParser::parseAsciiFromString(const std::string& content) {
    STLMesh mesh;
    if (trim(content).empty()) {
        mesh.isValid = false;
        mesh.errorMessage = "Empty file: Content contains no text or data.";
        return mesh;
    }

    std::istringstream stream(content);
    std::string line;
    bool inSolid = false;
    bool inFacet = false;
    bool inLoop = false;
    Triangle currentTri;
    int vertexCount = 0;
    size_t lineNum = 0;

    while (std::getline(stream, line)) {
        lineNum++;
        std::string trimmed = trim(line);
        if (trimmed.empty()) continue;

        std::string lower = toLower(trimmed);

        if (!inSolid) {
            if (lower.rfind("solid", 0) == 0) {
                inSolid = true;
                if (trimmed.length() > 5) {
                    mesh.solidName = trim(trimmed.substr(5));
                }
                if (mesh.solidName.empty()) mesh.solidName = "UnnamedSolid";
                continue;
            } else {
                mesh.isValid = false;
                mesh.errorMessage = "Line " + std::to_string(lineNum) +
                                    ": Missing 'solid' header keyword. File is not a valid ASCII STL.";
                return mesh;
            }
        }

        if (lower.rfind("endsolid", 0) == 0) {
            inSolid = false;
            break;
        }

        if (!inFacet) {
            if (lower.rfind("facet normal", 0) == 0) {
                inFacet = true;
                std::istringstream iss(trimmed.substr(12));
                if (!(iss >> currentTri.normal.x >> currentTri.normal.y >> currentTri.normal.z)) {
                    mesh.isValid = false;
                    mesh.errorMessage = "Line " + std::to_string(lineNum) +
                                        ": Malformed facet normal values.";
                    return mesh;
                }
                if (!currentTri.normal.isFinite()) {
                    mesh.isValid = false;
                    mesh.errorMessage = "Line " + std::to_string(lineNum) +
                                        ": Non-finite facet normal detected (NaN or Inf).";
                    return mesh;
                }
                vertexCount = 0;
                continue;
            }
        } else {
            // We are inside facet
            if (lower.rfind("endfacet", 0) == 0) {
                inFacet = false;
                if (vertexCount != 3) {
                    mesh.isValid = false;
                    mesh.errorMessage = "Line " + std::to_string(lineNum) +
                                        ": Facet ended with invalid vertex count: " + std::to_string(vertexCount);
                    return mesh;
                }

                // Compute normal if zero
                float ax = currentTri.v2.x - currentTri.v1.x;
                float ay = currentTri.v2.y - currentTri.v1.y;
                float az = currentTri.v2.z - currentTri.v1.z;
                float bx = currentTri.v3.x - currentTri.v1.x;
                float by = currentTri.v3.y - currentTri.v1.y;
                float bz = currentTri.v3.z - currentTri.v1.z;
                float cx = ay * bz - az * by;
                float cy = az * bx - ax * bz;
                float cz = ax * by - ay * bx;
                float len = std::sqrt(cx * cx + cy * cy + cz * cz);
                if (len > 1e-12f) {
                    if (std::abs(currentTri.normal.x) < 1e-6f &&
                        std::abs(currentTri.normal.y) < 1e-6f &&
                        std::abs(currentTri.normal.z) < 1e-6f) {
                        currentTri.normal = {cx / len, cy / len, cz / len};
                    }
                }
                mesh.triangles.push_back(currentTri);
                continue;
            }

            if (!inLoop) {
                if (lower.rfind("outer loop", 0) == 0) {
                    inLoop = true;
                    continue;
                } else {
                    mesh.isValid = false;
                    mesh.errorMessage = "Line " + std::to_string(lineNum) +
                                        ": Expected 'outer loop' or 'endfacet' inside facet.";
                    return mesh;
                }
            } else {
                if (lower.rfind("vertex", 0) == 0) {
                    std::istringstream iss(trimmed.substr(6));
                    Vector3 v;
                    if (!(iss >> v.x >> v.y >> v.z)) {
                        mesh.isValid = false;
                        mesh.errorMessage = "Line " + std::to_string(lineNum) +
                                            ": Malformed vertex coordinates.";
                        return mesh;
                    }
                    if (!v.isFinite()) {
                        mesh.isValid = false;
                        mesh.errorMessage = "Line " + std::to_string(lineNum) +
                                            ": Non-finite vertex coordinates (NaN or Inf).";
                        return mesh;
                    }

                    if (vertexCount == 0) currentTri.v1 = v;
                    else if (vertexCount == 1) currentTri.v2 = v;
                    else if (vertexCount == 2) currentTri.v3 = v;
                    vertexCount++;
                    continue;
                } else if (lower.rfind("endloop", 0) == 0) {
                    inLoop = false;
                    if (vertexCount != 3) {
                        mesh.isValid = false;
                        mesh.errorMessage = "Line " + std::to_string(lineNum) +
                                            ": Triangle facet must have exactly 3 vertices, found " +
                                            std::to_string(vertexCount);
                        return mesh;
                    }
                    continue;
                }
            }
        }
    }

    if (inFacet || inLoop) {
        mesh.isValid = false;
        mesh.errorMessage = "Unexpected end of file while parsing facet/loop.";
        return mesh;
    }

    if (mesh.triangles.empty()) {
        mesh.isValid = false;
        mesh.errorMessage = "Empty geometry: No triangle facets found in the file.";
        return mesh;
    }

    mesh.triangleCount = mesh.triangles.size();
    mesh.isValid = validateAndComputeBounds(mesh);
    return mesh;
}

bool STLParser::validateAndComputeBounds(STLMesh& mesh) {
    if (mesh.triangles.empty()) return false;

    float minX = std::numeric_limits<float>::max();
    float minY = std::numeric_limits<float>::max();
    float minZ = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::lowest();
    float maxY = std::numeric_limits<float>::lowest();
    float maxZ = std::numeric_limits<float>::lowest();

    auto updateBounds = [&](const Vector3& v) {
        if (v.x < minX) minX = v.x;
        if (v.y < minY) minY = v.y;
        if (v.z < minZ) minZ = v.z;
        if (v.x > maxX) maxX = v.x;
        if (v.y > maxY) maxY = v.y;
        if (v.z > maxZ) maxZ = v.z;
    };

    for (const auto& tri : mesh.triangles) {
        updateBounds(tri.v1);
        updateBounds(tri.v2);
        updateBounds(tri.v3);
    }

    mesh.bboxMin = {minX, minY, minZ};
    mesh.bboxMax = {maxX, maxY, maxZ};
    mesh.center = {0.5f * (minX + maxX), 0.5f * (minY + maxY), 0.5f * (minZ + maxZ)};

    float dx = maxX - minX;
    float dy = maxY - minY;
    float dz = maxZ - minZ;
    mesh.boundingSphereRadius = 0.5f * std::sqrt(dx * dx + dy * dy + dz * dz);

    return true;
}

std::string STLMesh::toJson() const {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(5);
    ss << "{\n";
    ss << "  \"valid\": " << (isValid ? "true" : "false") << ",\n";
    ss << "  \"error\": \"" << errorMessage << "\",\n";
    ss << "  \"solidName\": \"" << solidName << "\",\n";
    ss << "  \"triangleCount\": " << triangleCount << ",\n";
    ss << "  \"bounds\": {\n";
    ss << "    \"min\": [" << bboxMin.x << ", " << bboxMin.y << ", " << bboxMin.z << "],\n";
    ss << "    \"max\": [" << bboxMax.x << ", " << bboxMax.y << ", " << bboxMax.z << "],\n";
    ss << "    \"center\": [" << center.x << ", " << center.y << ", " << center.z << "],\n";
    ss << "    \"radius\": " << boundingSphereRadius << "\n";
    ss << "  },\n";
    ss << "  \"triangles\": [\n";

    for (size_t i = 0; i < triangles.size(); ++i) {
        const auto& t = triangles[i];
        ss << "    {"
           << "\"n\":[" << t.normal.x << "," << t.normal.y << "," << t.normal.z << "],"
           << "\"v\":[" << t.v1.x << "," << t.v1.y << "," << t.v1.z << ","
                        << t.v2.x << "," << t.v2.y << "," << t.v2.z << ","
                        << t.v3.x << "," << t.v3.y << "," << t.v3.z << "]}";
        if (i + 1 < triangles.size()) ss << ",";
        ss << "\n";
    }
    ss << "  ]\n";
    ss << "}";
    return ss.str();
}

} // namespace Workbench
