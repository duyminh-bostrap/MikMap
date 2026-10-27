/* ════════════════════════════════════════════════════════════════════════
 *  io/plugin/mikmap_sensor_plugin.h — ABI C cho plugin cảm biến của MikMap
 *
 *  Đây là file DUY NHẤT người viết plugin cần. C thuần (C89/C99/C11 đều
 *  biên dịch được), không include C++, không phụ thuộc MikMap. Nhờ vậy
 *  plugin có thể build bằng bất kỳ compiler nào (MSVC, GCC, Clang) và
 *  MikMap chỉ nạp nó qua 9 hàm extern "C" bên dưới.
 *
 *  Không dùng CHOP_CPlusPlusBase.h của TouchDesigner: header đó có
 *  "Shared Use License", chỉ được dùng kèm TouchDesigner.
 *
 *  Hợp đồng thread: host gọi api_version/name/default_config từ thread
 *  chính lúc nạp; MỌI hàm còn lại chỉ từ MỘT thread của PluginSource.
 *  Plugin không cần thread-safe.
 * ════════════════════════════════════════════════════════════════════════ */
#ifndef MIKMAP_SENSOR_PLUGIN_H
#define MIKMAP_SENSOR_PLUGIN_H

#define MIKMAP_SENSOR_API 1

/* Xuất hàm khỏi DLL. Trên Windows phải có __declspec(dllexport); trên
 * GCC/Clang visibility mặc định đã là public nhưng ghi rõ cho chắc. */
#if defined(_WIN32)
#  define MIKMAP_SENSOR_EXPORT __declspec(dllexport)
#elif defined(__GNUC__)
#  define MIKMAP_SENSOR_EXPORT __attribute__((visibility("default")))
#else
#  define MIKMAP_SENSOR_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Phải trả về MIKMAP_SENSOR_API. Host từ chối plugin lệch phiên bản. */
MIKMAP_SENSOR_EXPORT int mikmap_sensor_api_version(void);

/* Tên hiện trong menu "+ Add device", ví dụ "YDLIDAR". UTF-8. */
MIKMAP_SENSOR_EXPORT const char* mikmap_sensor_name(void);

/* JSON điền sẵn vào ô config của thiết bị mới. UTF-8. */
MIKMAP_SENSOR_EXPORT const char* mikmap_sensor_default_config(void);

/* NULL nếu config sai (host báo "plugin rejected config"). */
MIKMAP_SENSOR_EXPORT void* mikmap_sensor_create(const char* configJson);

/* 0 = ok, khác 0 = lỗi (đọc mikmap_sensor_error). */
MIKMAP_SENSOR_EXPORT int mikmap_sensor_start(void* s);

/* >0: số điểm của MỘT scan mới · 0: chưa có scan mới · <0: lỗi, host sẽ
 * dừng rồi tạo lại.
 *
 * Được phép chặn, nhưng PHẢI trả về trong ≤ 1000 ms (host join thread
 * khi stop()). rangeMm == 0 nghĩa là tia không hợp lệ.
 * angleRad: 0 = phía trước, tăng NGƯỢC chiều kim đồng hồ (cùng quy ước
 * với Hokuyo). Tối đa maxPoints điểm; host cấp mảng đủ maxPoints. */
MIKMAP_SENSOR_EXPORT int mikmap_sensor_poll(void* s, float* angleRad, float* rangeMm, int maxPoints);

/* Chuỗi sống tới lần gọi kế tiếp; "" nếu không lỗi. UTF-8. */
MIKMAP_SENSOR_EXPORT const char* mikmap_sensor_error(void* s);

MIKMAP_SENSOR_EXPORT void mikmap_sensor_stop(void* s);
MIKMAP_SENSOR_EXPORT void mikmap_sensor_destroy(void* s);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* MIKMAP_SENSOR_PLUGIN_H */
