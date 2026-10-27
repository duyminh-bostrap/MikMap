#include "io/plugin/PluginLoader.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <system_error>
#include <utility>

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#else
#  include <dlfcn.h>
#endif

namespace mikmap {
namespace {

// Giữ khớp MIKMAP_SENSOR_API trong io/plugin/mikmap_sensor_plugin.h.
constexpr int kApiVersion = 1;

#ifdef _WIN32
std::wstring widen(const std::string& utf8) {
    if (utf8.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    if (n <= 0) return {};
    std::wstring w(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), w.data(), n);
    return w;
}

std::string narrow(const std::filesystem::path& p) {
    const std::wstring w = p.wstring();
    if (w.empty()) return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    if (n <= 0) return {};
    std::string s(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), s.data(), n, nullptr, nullptr);
    return s;
}

std::filesystem::path fsPath(const std::string& utf8) { return std::filesystem::path(widen(utf8)); }
#else
std::filesystem::path fsPath(const std::string& utf8) { return std::filesystem::path(utf8); }
std::string narrow(const std::filesystem::path& p) { return p.string(); }
#endif

void* openLibrary(const std::string& path, std::string& why) {
#ifdef _WIN32
    const std::filesystem::path p = fsPath(path);
    HMODULE h = LoadLibraryExW(p.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!h) why = std::system_category().message(static_cast<int>(GetLastError()));
    return reinterpret_cast<void*>(h);
#else
    void* h = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!h) {
        const char* e = dlerror();
        why = e ? e : "dlopen failed";
    }
    return h;
#endif
}

void closeLibrary(void* handle) {
#ifdef _WIN32
    if (handle) FreeLibrary(static_cast<HMODULE>(handle));
#else
    if (handle) dlclose(handle);
#endif
}

void* symbol(void* handle, const char* name) {
#ifdef _WIN32
    FARPROC proc = GetProcAddress(static_cast<HMODULE>(handle), name);
    void* p = nullptr;
    static_assert(sizeof p == sizeof proc, "function pointer and void* must have the same size");
    std::memcpy(&p, &proc, sizeof p);
    return p;
#else
    return dlsym(handle, name);
#endif
}

template <typename F>
bool fetch(void* handle, const char* name, F& fn, std::string& why) {
    void* p = symbol(handle, name);
    if (!p) {
        why = std::string("missing export ") + name;
        return false;
    }
    static_assert(sizeof(F) == sizeof(void*), "function pointer and void* must have the same size");
    std::memcpy(&fn, &p, sizeof fn);
    return true;
}

} // namespace

bool loadSensorPlugin(const std::string& path, SensorPlugin& out, std::string& err) {
    out = SensorPlugin{};
    err.clear();

    std::error_code ec;
    const auto abs = std::filesystem::absolute(fsPath(path), ec);
    const std::string full = ec ? path : narrow(abs);

    std::string why;
    void* handle = openLibrary(full, why);
    if (!handle) {
        err = path + ": " + why;
        return false;
    }

    SensorPlugin p;
    p.path = full;
    p.handle = handle;

    if (!fetch(handle, "mikmap_sensor_api_version", p.api_version, why) ||
        !fetch(handle, "mikmap_sensor_name", p.plugin_name, why) ||
        !fetch(handle, "mikmap_sensor_default_config", p.default_config, why) ||
        !fetch(handle, "mikmap_sensor_create", p.create, why) ||
        !fetch(handle, "mikmap_sensor_start", p.start, why) ||
        !fetch(handle, "mikmap_sensor_poll", p.poll, why) ||
        !fetch(handle, "mikmap_sensor_error", p.error, why) ||
        !fetch(handle, "mikmap_sensor_stop", p.stop, why) ||
        !fetch(handle, "mikmap_sensor_destroy", p.destroy, why)) {
        closeLibrary(handle);
        err = path + ": " + why;
        return false;
    }

    const int version = p.api_version();
    if (version != kApiVersion) {
        closeLibrary(handle);
        err = path + ": API version " + std::to_string(version) + ", MikMap needs " + std::to_string(kApiVersion);
        return false;
    }

    const char* name = p.plugin_name();
    if (!name || !*name) {
        closeLibrary(handle);
        err = path + ": plugin returned an empty name";
        return false;
    }
    const char* cfg = p.default_config();
    p.name = name;
    p.defaultConfig = cfg ? cfg : "";

    out = std::move(p);
    return true;
}

std::vector<SensorPlugin> scanPluginDir(const std::string& dir, std::vector<std::string>& errors) {
    std::vector<SensorPlugin> out;

    std::error_code ec;
    std::filesystem::directory_iterator it(fsPath(dir), ec);
    if (ec) return out;   // chưa cài plugin (hoặc không có quyền): không phải lỗi

    const std::filesystem::directory_iterator end;
    for (; it != end; it.increment(ec)) {
        if (ec) break;

        std::error_code fec;
        if (!it->is_regular_file(fec) || fec) continue;

        std::string ext = narrow(it->path().extension());
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (ext != ".dll" && ext != ".so" && ext != ".dylib") continue;

        const std::string path = narrow(it->path());
        SensorPlugin p;
        std::string err;
        if (loadSensorPlugin(path, p, err)) out.push_back(std::move(p));
        else errors.push_back(std::move(err));
    }

    std::sort(out.begin(), out.end(), [](const SensorPlugin& a, const SensorPlugin& b) { return a.path < b.path; });
    return out;
}

} // namespace mikmap
