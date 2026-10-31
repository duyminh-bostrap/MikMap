#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
namespace mikmap {
enum class OrtDevice { Auto, CudaOnly, Cpu };
struct OrtTensorInfo { std::string name; std::vector<int64_t> shape; };
class OrtRuntime {
public:
    OrtRuntime();
    ~OrtRuntime();
    OrtRuntime(const OrtRuntime&) = delete;
    OrtRuntime& operator=(const OrtRuntime&) = delete;
    bool load(const std::string& runtimeDirUtf8, std::string& err);
    bool createSession(const std::string& modelPathUtf8, OrtDevice dev, std::string& err, bool* usedCuda = nullptr);
    std::vector<OrtTensorInfo> inputs() const;
    std::vector<OrtTensorInfo> outputs() const;
    bool run(const float* input, const std::vector<int64_t>& shape, std::vector<std::vector<float>>& outputs, std::vector<std::vector<int64_t>>& outShapes, std::string& err);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
