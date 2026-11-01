#include "io/ai/OrtRuntime.h"
#include "onnxruntime_c_api.h"
#include <cstring>
#include <filesystem>
#include <limits>
#include <stdexcept>
#include <system_error>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace mikmap {
namespace {
std::filesystem::path utf8Path(const std::string& s) {
    if (s.empty() || s.find('\0') != std::string::npos) throw std::runtime_error("Empty path or embedded NUL");
    return std::filesystem::path(std::u8string(s.begin(), s.end()));
}
template<class T> struct Handle {
    T* p = nullptr;
    void (ORT_API_CALL *release)(T*);
    explicit Handle(void (ORT_API_CALL *fn)(T*)) : release(fn) {}
    ~Handle() { if (p) release(p); }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
};
size_t elementCount(const std::vector<int64_t>& shape) {
    size_t count = 1;
    for (const auto d : shape) {
        if (d <= 0 || static_cast<uint64_t>(d) > std::numeric_limits<size_t>::max() / count)
            throw std::runtime_error("Tensor dimensions must be positive and fit in memory");
        count *= static_cast<size_t>(d);
    }
    if (count > std::numeric_limits<size_t>::max() / sizeof(float)) throw std::runtime_error("Tensor byte size overflow");
    return count;
}
}
struct OrtRuntime::Impl {
    const OrtApi* api = nullptr;
    OrtEnv* env = nullptr;
    OrtSession* session = nullptr;
    std::vector<OrtTensorInfo> in, out;
    std::vector<const char*> inNames, outNames;
    ~Impl() {
        if (session) api->ReleaseSession(session);
        if (env) api->ReleaseEnv(env);
        // DLLs stay loaded for process lifetime, including failed API negotiation.
    }
    void check(OrtStatus* status) const {
        if (!status) return;
        const std::string message = api->GetErrorMessage(status);
        api->ReleaseStatus(status);
        throw std::runtime_error(message);
    }
    std::vector<int64_t> dimensions(const OrtTensorTypeAndShapeInfo* info) const {
        ONNXTensorElementDataType type{};
        check(api->GetTensorElementType(info, &type));
        if (type != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT) throw std::runtime_error("Only float32 tensors are supported");
        size_t rank = 0; check(api->GetDimensionsCount(info, &rank));
        std::vector<int64_t> shape(rank); check(api->GetDimensions(info, shape.data(), rank));
        return shape;
    }
    std::vector<OrtTensorInfo> describe(OrtSession* s, bool input) const {
        size_t count = 0;
        check(input ? api->SessionGetInputCount(s, &count) : api->SessionGetOutputCount(s, &count));
        if ((input && count != 1) || (!input && count == 0)) throw std::runtime_error("Expected one input and at least one output");
        OrtAllocator* allocator = nullptr; check(api->GetAllocatorWithDefaultOptions(&allocator));
        std::vector<OrtTensorInfo> result;
        for (size_t i = 0; i < count; ++i) {
            char* raw = nullptr;
            check(input ? api->SessionGetInputName(s, i, allocator, &raw) : api->SessionGetOutputName(s, i, allocator, &raw));
            const auto freeName = [allocator](char* name) { allocator->Free(allocator, name); };
            std::unique_ptr<char, decltype(freeName)> name(raw, freeName);
            Handle<OrtTypeInfo> type(api->ReleaseTypeInfo);
            check(input ? api->SessionGetInputTypeInfo(s, i, &type.p) : api->SessionGetOutputTypeInfo(s, i, &type.p));
            const OrtTensorTypeAndShapeInfo* tensor = nullptr;
            check(api->CastTypeInfoToTensorInfo(type.p, &tensor));
            if (!tensor) throw std::runtime_error("Expected tensor input/output");
            result.push_back({name.get(), dimensions(tensor)});
        }
        return result;
    }
};
OrtRuntime::OrtRuntime() : impl_(std::make_unique<Impl>()) {}
OrtRuntime::~OrtRuntime() = default;

bool OrtRuntime::load(const std::string& dir, std::string& err) {
    err.clear();
    try {
        const auto path = utf8Path(dir);
        if (!path.is_absolute()) throw std::runtime_error("Runtime directory must be absolute");
#ifdef _WIN32
        const auto dll = path / "onnxruntime.dll";
        // Invalid packs must return an error without opening a Windows loader dialog.
        DWORD previousMode = 0;
        const BOOL changedMode = SetThreadErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX, &previousMode);
        const HMODULE module = LoadLibraryExW(dll.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        const DWORD loadError = GetLastError();
        if (changedMode) SetThreadErrorMode(previousMode, nullptr);
        if (!module) throw std::runtime_error(std::system_category().message(static_cast<int>(loadError)));
        const auto symbol = GetProcAddress(module, "OrtGetApiBase");
        if (!symbol) throw std::runtime_error("Missing OrtGetApiBase export");
        decltype(&OrtGetApiBase) getBase = nullptr;
        static_assert(sizeof(getBase) == sizeof(symbol));
        std::memcpy(&getBase, &symbol, sizeof(getBase));
        const OrtApiBase* base = getBase();
        const OrtApi* api = base ? base->GetApi(ORT_API_VERSION) : nullptr;
        if (!api) throw std::runtime_error("Runtime API is older than the vendored ORT API " + std::to_string(ORT_API_VERSION));
        auto next = std::make_unique<Impl>(); next->api = api;
        next->check(api->CreateEnv(ORT_LOGGING_LEVEL_WARNING, "MikMap", &next->env));
        impl_ = std::move(next);
        return true;
#else
        throw std::runtime_error("ONNX Runtime pack loading is supported on Windows only");
#endif
    } catch (const std::exception& e) { err = "ONNX Runtime '" + dir + "': " + e.what(); return false; }
}
bool OrtRuntime::createSession(const std::string& model, OrtDevice dev, std::string& err, bool* usedCuda) {
    err.clear(); if (usedCuda) *usedCuda = false;
    auto& r = *impl_;
    if (!r.env) { err = "ONNX Runtime is not loaded"; return false; }
    try {
        const auto path = utf8Path(model);
        const auto attempt = [&](bool cuda) {
            Handle<OrtSessionOptions> options(r.api->ReleaseSessionOptions);
            r.check(r.api->CreateSessionOptions(&options.p));
            if (cuda) {
                Handle<OrtCUDAProviderOptionsV2> provider(r.api->ReleaseCUDAProviderOptions);
                r.check(r.api->CreateCUDAProviderOptions(&provider.p));
                r.check(r.api->SessionOptionsAppendExecutionProvider_CUDA_V2(options.p, provider.p));
                if (dev == OrtDevice::CudaOnly)
                    r.check(r.api->AddSessionConfigEntry(options.p, "session.disable_cpu_ep_fallback", "1"));
            }
            Handle<OrtSession> session(r.api->ReleaseSession);
            r.check(r.api->CreateSession(r.env, path.c_str(), options.p, &session.p));
            auto in = r.describe(session.p, true), out = r.describe(session.p, false);
            std::vector<const char*> inNames, outNames;
            for (const auto& info : in) inNames.push_back(info.name.c_str());
            for (const auto& info : out) outNames.push_back(info.name.c_str());
            if (r.session) r.api->ReleaseSession(r.session);
            r.session = session.p; session.p = nullptr;
            r.in = std::move(in); r.out = std::move(out);
            r.inNames = std::move(inNames); r.outNames = std::move(outNames);
            if (usedCuda) *usedCuda = cuda;
        };
        if (dev == OrtDevice::Auto) {
            try { attempt(true); } catch (const std::exception&) { attempt(false); }
        } else { attempt(dev == OrtDevice::CudaOnly); }
        return true;
    } catch (const std::exception& e) { err = "ONNX model '" + model + "': " + e.what(); return false; }
}
std::vector<OrtTensorInfo> OrtRuntime::inputs() const { return impl_->in; }
std::vector<OrtTensorInfo> OrtRuntime::outputs() const { return impl_->out; }
bool OrtRuntime::run(const float* input, const std::vector<int64_t>& shape,
                     std::vector<std::vector<float>>& outputs,
                     std::vector<std::vector<int64_t>>& outShapes, std::string& err) {
    err.clear(); auto& r = *impl_;
    if (!r.session) { err = "ONNX session is not created"; return false; }
    try {
        if (!input) throw std::runtime_error("Input tensor is null");
        if (shape.size() != r.in[0].shape.size()) throw std::runtime_error("Input tensor rank mismatch");
        for (size_t i = 0; i < shape.size(); ++i)
            if (r.in[0].shape[i] >= 0 && r.in[0].shape[i] != shape[i]) throw std::runtime_error("Input tensor shape mismatch");
        const size_t count = elementCount(shape);
        Handle<OrtMemoryInfo> memory(r.api->ReleaseMemoryInfo);
        r.check(r.api->CreateCpuMemoryInfo(OrtArenaAllocator, OrtMemTypeDefault, &memory.p));
        Handle<OrtValue> value(r.api->ReleaseValue);
        r.check(r.api->CreateTensorWithDataAsOrtValue(memory.p, const_cast<float*>(input), count * sizeof(float), shape.data(), shape.size(), ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT, &value.p));
        struct Results {
            const OrtApi* api;
            std::vector<OrtValue*> values;
            ~Results() { for (auto* v : values) if (v) api->ReleaseValue(v); }
        } result{r.api, std::vector<OrtValue*>(r.out.size(), nullptr)};
        const OrtValue* inValue = value.p;
        r.check(r.api->Run(r.session, nullptr, r.inNames.data(), &inValue, 1, r.outNames.data(), r.outNames.size(), result.values.data()));
        outputs.resize(result.values.size()); outShapes.resize(result.values.size());
        for (size_t i = 0; i < result.values.size(); ++i) {
            Handle<OrtTensorTypeAndShapeInfo> type(r.api->ReleaseTensorTypeAndShapeInfo);
            r.check(r.api->GetTensorTypeAndShape(result.values[i], &type.p));
            outShapes[i] = r.dimensions(type.p);
            size_t size = 0; r.check(r.api->GetTensorShapeElementCount(type.p, &size));
            if (size > std::numeric_limits<size_t>::max() / sizeof(float)) throw std::runtime_error("Output tensor byte size overflow");
            float* data = nullptr; r.check(r.api->GetTensorMutableData(result.values[i], reinterpret_cast<void**>(&data)));
            outputs[i].resize(size);
            if (size) std::memcpy(outputs[i].data(), data, size * sizeof(float));
        }
        return true;
    } catch (const std::exception& e) { err = e.what(); return false; }
}
}
