#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
namespace mikmap {
enum class OrtDevice { Auto, CudaOnly, Cpu };
enum class OrtTensorType { Float32, Int64 };
struct OrtTensorInfo { std::string name; std::vector<int64_t> shape; OrtTensorType type=OrtTensorType::Float32; };
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
    // Missing keys return false, clear value and report "Metadata key not found".
    bool customMetadata(const std::string& key, std::string& value, std::string& err) const;
    // Caller supplies at least product(shape) contiguous floats for the entire call.
    // Serialize all access to this instance externally, including load/session/run/destruction.
    bool run(const float* input, const std::vector<int64_t>& shape, std::vector<std::vector<float>>& outputs, std::vector<std::vector<int64_t>>& outShapes, std::string& err);
    // Buffers share output indices; only the vector matching the tensor type is populated.
    bool run(const float* input, const std::vector<int64_t>& shape, std::vector<std::vector<float>>& outputs, std::vector<std::vector<int64_t>>& integerOutputs, std::vector<std::vector<int64_t>>& outShapes, std::string& err);
private:
    bool runImpl(const float*, const std::vector<int64_t>&, std::vector<std::vector<float>>&, std::vector<std::vector<int64_t>>*, std::vector<std::vector<int64_t>>&, std::string&);
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
