#pragma once

#include <string>
#include <vector>
#include <cmath>
#include <limits>
#include <sstream>

namespace Workbench {

struct Vector3 {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};

    bool isFinite() const {
        return std::isfinite(x) && std::isfinite(y) && std::isfinite(z);
    }
};

struct Triangle {
    Vector3 normal;
    Vector3 v1;
    Vector3 v2;
    Vector3 v3;
};

struct STLMesh {
    std::string solidName;
    std::vector<Triangle> triangles;
    Vector3 bboxMin;
    Vector3 bboxMax;
    Vector3 center;
    float boundingSphereRadius{0.0f};
    size_t triangleCount{0};
    bool isValid{false};
    std::string errorMessage;

    std::string toJson() const;
};

class STLParser {
public:
    static STLMesh parseAscii(const std::string& filePath);
    static STLMesh parseAsciiFromString(const std::string& content);

private:
    static bool validateAndComputeBounds(STLMesh& mesh);
};

} // namespace Workbench
