// ════════════════════════════════════════════════════════════════════════
//  io/camera/CameraDevice.h — giao diện camera, không phụ thuộc hãng
//
//  Camera 2D (Media Foundation) hôm nay và Camera 3D (SDK của hãng) ngày
//  mai cùng cài ICameraDevice. Nhờ vậy CameraSource, bảng khả năng và UI
//  không cần biết ảnh đến từ webcam hay RealSense.
//
//  ── Hợp đồng luồng ───────────────────────────────────────────────────
//    · open()/close()  gọi từ thread điều khiển (trước khi chạy / khi dừng)
//    · read()          CHẶN, chỉ gọi từ thread capture
//    · CameraFrame giữ bộ đệm của chính nó và producer DÙNG LẠI: read()
//      chỉ cấp phát khi kích thước đổi, không cấp phát mỗi khung sau khi
//      đã ổn định (yêu cầu của vòng lặp nóng)
//
//  Đơn vị khai báo là LUỒNG (Color/Depth/IR): mỗi luồng có kích thước và
//  fps riêng. Một chế độ (CamMode) chỉ dùng được nếu thiết bị có đủ luồng
//  mà chế độ đó cần — xem modeSupported().
// ════════════════════════════════════════════════════════════════════════
#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace mikmap {

/// Thông số một luồng. width/height/fps = 0 nghĩa là thiết bị không khai.
struct StreamInfo {
    int width = 0, height = 0;
    float fps = 0;
};

struct CameraCaps {
    bool hasColor = false, hasDepth = false, hasIR = false;
    StreamInfo color, depth, ir;
};

/// Một khung đã giải mã. Bộ đệm thuộc về người gọi và được read() dùng lại.
struct CameraFrame {
    int width = 0, height = 0;
    /// RGB24, thứ tự byte R,G,B (R-first — KHÔNG phải BGR của DIB), đóng gói
    /// liền width*height*3. Mọi backend (FakeCamera, MfCamera) phải theo đúng
    /// thứ tự này; texture upload dùng GL_RGB.
    std::vector<uint8_t> rgb;
    int depthW = 0, depthH = 0;
    std::vector<uint16_t> depthMm;     ///< rỗng với Camera 2D
    int64_t tCaptureNs = 0;            ///< mốc thời gian lúc thiết bị bắt khung
    uint64_t index = 0;                ///< số thứ tự khung, tăng dần
};

/// Một thiết bị nhìn thấy được. id bền qua các lần chạy, name để hiện UI.
struct CameraInfo {
    std::string id, name;
};

class ICameraDevice {
public:
    virtual ~ICameraDevice() = default;

    /// Mở theo id (chuỗi rỗng = thiết bị đầu tiên). width/height là gợi ý;
    /// thiết bị có thể chọn kích thước gần nhất. Lỗi ghi vào err.
    virtual bool open(const std::string& id, int width, int height, std::string& err) = 0;

    virtual void close() = 0;

    /// Khả năng hiện tại. Gọi được cả trước khi open (sau khi dò thiết bị).
    virtual CameraCaps caps() const = 0;

    /// Chờ tối đa timeoutMs cho khung kế tiếp và ghi vào out.
    /// CHẶN — chỉ gọi từ thread capture. false kèm err khi hết giờ hoặc
    /// thiết bị bị rút.
    virtual bool read(CameraFrame& out, int timeoutMs, std::string& err) = 0;
};

/// Chế độ xử lý của một camera.
enum class CamMode {
    Off = 0,    ///< chỉ xem trước, không suy luận
    Body = 1,   ///< nhận dạng thân người (cần luồng Color)
    Hand = 2,
    Face = 3,
    Full = 4,
};

/// Bảng khả năng: chế độ m có dùng được với caps c không? Nếu không, why
/// nói thiếu luồng nào (tiếng Anh — hiện thẳng lên UI).
bool modeSupported(CamMode m, const CameraCaps& c, std::string& why);

// ────────────────────────────────────────────────────────────────────────
//  FakeCamera — thiết bị giả cho test, không cần phần cứng
//
//  Kịch bản khung được phát đúng thứ tự; hết kịch bản thì read() chờ tới
//  timeout rồi báo lỗi (hoặc quay vòng nếu setLoop(true)). setUnplugAfter()
//  mô phỏng rút cáp giữa buổi; replug() cắm lại và chạy lại kịch bản.
//  Mọi hàm an toàn khi gọi từ thread khác (read() chặn trên thread capture).
// ────────────────────────────────────────────────────────────────────────
class FakeCamera final : public ICameraDevice {
public:
    struct Frame {
        int width = 0, height = 0;
        std::vector<uint8_t> rgb;
        int depthW = 0, depthH = 0;
        std::vector<uint16_t> depthMm;
        int64_t tCaptureNs = 0;
    };

    /// Mặc định: Color 640×480 @30, chưa có kịch bản.
    FakeCamera();
    explicit FakeCamera(CameraCaps caps);
    FakeCamera(CameraCaps caps, std::vector<Frame> frames);

    bool open(const std::string& id, int width, int height, std::string& err) override;
    void close() override;
    CameraCaps caps() const override;
    bool read(CameraFrame& out, int timeoutMs, std::string& err) override;

    // ── Kịch bản cho test ──────────────────────────────────────────────
    void setCaps(const CameraCaps& c);

    /// Thay kịch bản; lần đọc kế tiếp bắt đầu từ khung 0.
    void setFrames(std::vector<Frame> frames);

    /// Hết kịch bản thì quay lại khung 0 thay vì chờ timeout.
    void setLoop(bool loop);

    /// Giãn cách giữa hai khung (ms), mô phỏng nhịp camera thật.
    /// 0 = có ngay. timeoutMs vẫn là trần chờ của read().
    void setFrameIntervalMs(int ms);

    /// Sau frames khung đã phát thì coi như rút cáp. < 0 = không rút.
    void setUnplugAfter(int frames);

    /// open() luôn thất bại với message này (mô phỏng app khác giữ camera).
    void setOpenError(std::string message);

    /// Cắm lại: xoá trạng thái rút, chạy lại kịch bản từ đầu.
    void replug();

    int framesDelivered() const;   ///< số khung đã trả kể từ lần open() thành công

private:
    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    CameraCaps m_caps;
    std::vector<Frame> m_frames;
    std::string m_openError;
    std::string m_id;
    size_t m_cursor = 0;
    int m_delivered = 0;
    int m_unplugAfter = -1;
    int m_frameIntervalMs = 0;
    bool m_open = false;
    bool m_plugged = true;
    bool m_loop = false;
    std::chrono::steady_clock::time_point m_nextDue{};
};

} // namespace mikmap
