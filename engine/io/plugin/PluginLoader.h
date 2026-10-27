// ════════════════════════════════════════════════════════════════════════
//  io/plugin/PluginLoader.h — nạp plugin cảm biến dạng DLL
//
//  Windows: LoadLibraryExW(path tuyệt đối, LOAD_WITH_ALTERED_SEARCH_PATH)
//  để DLL phụ thuộc nằm cạnh plugin (vd ydlidar_sdk.dll) được tìm thấy,
//  không phải chép vào cạnh mikmap.exe.
//  Linux/macOS: dlopen(RTLD_NOW | RTLD_LOCAL).
//
//  Thư viện KHÔNG BAO GIỜ được gỡ: con trỏ hàm trong SensorPlugin phải
//  còn hiệu lực suốt đời tiến trình. Vì vậy SensorPlugin copy được tự do.
// ════════════════════════════════════════════════════════════════════════
#pragma once

#include <string>
#include <vector>

namespace mikmap {

/// 9 hàm của ABI (xem io/plugin/mikmap_sensor_plugin.h) + thông tin nạp.
struct SensorPlugin {
    std::string path;           // đường dẫn tuyệt đối đã nạp
    std::string name;           // mikmap_sensor_name(), hiện trong menu
    std::string defaultConfig;  // mikmap_sensor_default_config()
    void* handle = nullptr;     // HMODULE / dlopen handle, không bao giờ đóng

    int         (*api_version)() = nullptr;
    const char* (*plugin_name)() = nullptr;
    const char* (*default_config)() = nullptr;
    void*       (*create)(const char* configJson) = nullptr;
    int         (*start)(void* s) = nullptr;
    int         (*poll)(void* s, float* angleRad, float* rangeMm, int maxPoints) = nullptr;
    const char* (*error)(void* s) = nullptr;
    void        (*stop)(void* s) = nullptr;
    void        (*destroy)(void* s) = nullptr;
};

/// Nạp một file plugin. false + err (có đường dẫn) nếu thiếu file/hàm hoặc
/// lệch phiên bản API.
bool loadSensorPlugin(const std::string& path, SensorPlugin& out, std::string& err);

/// Quét *.dll / *.so / *.dylib trong `dir` (không đệ quy), đã sắp xếp theo
/// đường dẫn. Mỗi plugin nạp lỗi được ghi một dòng vào `errors`; thư mục
/// không tồn tại chỉ trả về rỗng (chưa cài plugin là chuyện bình thường).
std::vector<SensorPlugin> scanPluginDir(const std::string& dir, std::vector<std::string>& errors);

} // namespace mikmap
