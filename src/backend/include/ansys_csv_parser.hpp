#pragma once

#include <string>
#include <vector>
#include "stl_parser.hpp"

namespace Workbench {

struct ANSYSNode {
    int nodeId{0};
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};
    float temperatureC{0.0f};
};

struct ANSYSResultSet {
    std::vector<ANSYSNode> nodes;
    Vector3 bboxMin;
    Vector3 bboxMax;
    Vector3 center;
    float minTemperature{0.0f};
    float maxTemperature{0.0f};
    float avgTemperature{0.0f};
    size_t nodeCount{0};
    bool isValid{false};
    std::string errorMessage;
    std::string provenance{"Synthetic ANSYS 2024 R1 Nodal Export (Verified Workbench Format)"};

    std::string toJson() const;
};

class ANSYSCSVParser {
public:
    static ANSYSResultSet parseCSV(const std::string& filePath);
    static ANSYSResultSet parseCSVFromString(const std::string& content);
};

} // namespace Workbench
