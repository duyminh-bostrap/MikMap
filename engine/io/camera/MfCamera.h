// ════════════════════════════════════════════════════════════════════════
//  io/camera/MfCamera.h — camera 2D qua Media Foundation (Windows)
//
//  Cài ICameraDevice bằng IMFSourceReader. Phần khai báo ở đây cố ý KHÔNG
//  kéo <mfapi.h>/<mfreadwrite.h> vào: người dùng CameraSource chỉ thấy một
//  interface sạch, và bản build ngoài Windows vẫn biên dịch được (mọi hàm
//  trả lỗi "unsupported").
//
//  ── Hợp đồng luồng (giống CameraDevice.h) ────────────────────────────
//    · open()/close() gọi từ thread điều khiển.
//    · read() CHẶN, chỉ gọi từ thread capture. close() đánh thức read()
//      đang chờ và chờ nó rời khỏi reader trước khi giải phóng.
//    · read() dùng lại out.rgb; chỉ cấp phát khi kích thước đổi.
// ════════════════════════════════════════════════════════════════════════
#pragma once

#include "io/camera/CameraDevice.h"

#include <memory>
#include <string>
#include <vector>

namespace mikmap {

/// Liệt kê camera 2D đang cắm. id là symbolic link của thiết bị (bền qua
/// các lần chạy), name là tên thân thiện để hiện UI. Không bao giờ ném.
/// Ngoài Windows: danh sách rỗng và err chứa "unsupported".
std::vector<CameraInfo> listCameras(std::string& err);

/// Camera 2D (webcam/UVC) qua Media Foundation.
///
/// open() nhận id do listCameras() trả về, hoặc chỉ số 0,1,2… (tiện cho
/// dòng lệnh), hoặc chuỗi rỗng (= camera đầu tiên). Nó chọn cỡ ảnh nhỏ
/// nhất vẫn ≥ cỡ yêu cầu; nếu không có cỡ nào đủ lớn thì lấy cỡ lớn nhất.
/// width/height ≤ 0 nghĩa là "không có yêu cầu" — lấy cỡ lớn nhất.
///
/// read() trả CameraFrame::rgb theo thứ tự R,G,B (R-first). MFVideoFormat_RGB24
/// của Media Foundation là BGR (DIB) nên đường copy nhanh phải đảo kênh; đường
/// NV12/YUY2 tự ghi R-first. Nhờ vậy mọi định dạng cho cùng một thứ tự byte.
class MfCamera final : public ICameraDevice {
public:
    MfCamera();
    ~MfCamera() override;

    MfCamera(const MfCamera&) = delete;
    MfCamera& operator=(const MfCamera&) = delete;

    bool open(const std::string& id, int width, int height, std::string& err) override;
    void close() override;
    CameraCaps caps() const override;
    bool read(CameraFrame& out, int timeoutMs, std::string& err) override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace mikmap
