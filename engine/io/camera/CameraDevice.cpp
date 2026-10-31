// ════════════════════════════════════════════════════════════════════════
//  io/camera/CameraDevice.cpp — bảng khả năng + camera giả cho test
//
//  Ở đây KHÔNG có API hệ điều hành nào: phần này thuần STL để test chạy
//  trên mọi nền tảng, kể cả CI không có webcam. MfCamera (Task 3) mới là
//  phần chạm Media Foundation.
// ════════════════════════════════════════════════════════════════════════
#include "io/camera/CameraDevice.h"

#include <algorithm>
#include <utility>

namespace mikmap {

// ────────────────────────────────────────────────────────────────────────
//  Bảng khả năng
// ────────────────────────────────────────────────────────────────────────

bool modeSupported(CamMode m, const CameraCaps& c, std::string& why) {
    switch (m) {
    case CamMode::Off:
        // Xem trước không cần suy luận: thiết bị nào cũng được.
        why.clear();
        return true;

    case CamMode::Body:
        // Mô hình thân người ăn ảnh RGB; Depth chỉ là tuỳ chọn (z theo mét).
        if (!c.hasColor) {
            why = "Body mode needs a Color stream";
            return false;
        }
        why.clear();
        return true;
    }

    why = "Unknown camera mode";
    return false;
}

// ────────────────────────────────────────────────────────────────────────
//  FakeCamera
// ────────────────────────────────────────────────────────────────────────

FakeCamera::FakeCamera() : FakeCamera(CameraCaps{true, false, false,
                                                 StreamInfo{640, 480, 30.0f},
                                                 StreamInfo{},
                                                 StreamInfo{}}) {}

FakeCamera::FakeCamera(CameraCaps caps) : m_caps(caps) {}

FakeCamera::FakeCamera(CameraCaps caps, std::vector<Frame> frames)
    : m_caps(caps), m_frames(std::move(frames)) {}

bool FakeCamera::open(const std::string& id, int width, int height, std::string& err) {
    std::lock_guard<std::mutex> lock(m_mutex);
    err.clear();

    if (!m_openError.empty()) {
        err = m_openError;
        return false;
    }
    if (!m_plugged) {
        err = "Camera not connected: " + id;
        return false;
    }

    // Kịch bản đã mang sẵn kích thước; width/height chỉ là gợi ý của caller
    // nên không cắt/co lại khung giả.
    (void)width;
    (void)height;

    m_id = id;
    m_open = true;
    m_cursor = 0;
    m_delivered = 0;
    m_nextDue = std::chrono::steady_clock::now();
    m_cv.notify_all();
    return true;
}

void FakeCamera::close() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_open = false;
    m_cv.notify_all();
}

CameraCaps FakeCamera::caps() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_caps;
}

bool FakeCamera::read(CameraFrame& out, int timeoutMs, std::string& err) {
    err.clear();

    const int waitMs = timeoutMs > 0 ? timeoutMs : 0;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(waitMs);

    std::unique_lock<std::mutex> lock(m_mutex);
    for (;;) {
        if (!m_open) {
            err = "Camera is not open";
            return false;
        }
        if (!m_plugged) {
            err = "Camera unplugged: " + m_id;
            return false;
        }

        // Đã phát đủ số khung thì "rút cáp" — giữ nguyên trạng thái cho tới
        // khi test gọi replug(), giống cắm lại một thiết bị thật.
        if (m_unplugAfter >= 0 && m_delivered >= m_unplugAfter) {
            m_plugged = false;
            err = "Camera unplugged: " + m_id;
            return false;
        }

        if (m_cursor >= m_frames.size() && m_loop && !m_frames.empty()) {
            m_cursor = 0;
        }

        const auto now = std::chrono::steady_clock::now();
        if (m_cursor < m_frames.size() && now >= m_nextDue) {
            break;   // có khung và đã tới hạn
        }

        // Chờ tới hạn của khung kế tiếp, hoặc hết timeout — cái nào trước.
        auto wake = deadline;
        if (m_cursor < m_frames.size() && m_nextDue < wake) {
            wake = m_nextDue;
        }

        // Rút cáp/đóng trong lúc chờ phải đánh thức ngay, không đợi hết giờ.
        if (m_cv.wait_until(lock, wake, [this] { return !m_open || !m_plugged; })) {
            continue;
        }
        const auto after = std::chrono::steady_clock::now();
        if (m_cursor < m_frames.size() && after >= m_nextDue) {
            continue;   // khung vừa tới hạn đúng lúc timeout — vẫn trả
        }
        if (after >= deadline) {
            err = "Camera: no frame within timeout";
            return false;
        }
    }

    const Frame& f = m_frames[m_cursor++];
    ++m_delivered;

    // Dùng lại bộ đệm của caller: chỉ resize khi kích thước đổi.
    out.width = f.width;
    out.height = f.height;
    if (out.rgb.size() != f.rgb.size()) {
        out.rgb.resize(f.rgb.size());
    }
    std::copy(f.rgb.begin(), f.rgb.end(), out.rgb.begin());

    out.depthW = f.depthW;
    out.depthH = f.depthH;
    if (out.depthMm.size() != f.depthMm.size()) {
        out.depthMm.resize(f.depthMm.size());
    }
    std::copy(f.depthMm.begin(), f.depthMm.end(), out.depthMm.begin());

    out.tCaptureNs = f.tCaptureNs;
    out.index = static_cast<uint64_t>(m_delivered - 1);

    m_nextDue = std::chrono::steady_clock::now() +
                std::chrono::milliseconds(m_frameIntervalMs > 0 ? m_frameIntervalMs : 0);
    return true;
}

void FakeCamera::setCaps(const CameraCaps& c) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_caps = c;
}

void FakeCamera::setFrames(std::vector<Frame> frames) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_frames = std::move(frames);
    m_cursor = 0;
    m_delivered = 0;
    m_nextDue = std::chrono::steady_clock::now();
    m_cv.notify_all();
}

void FakeCamera::setLoop(bool loop) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_loop = loop;
    m_cv.notify_all();
}

void FakeCamera::setFrameIntervalMs(int ms) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_frameIntervalMs = ms > 0 ? ms : 0;
    m_nextDue = std::chrono::steady_clock::now();
    m_cv.notify_all();
}

void FakeCamera::setUnplugAfter(int frames) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_unplugAfter = frames;
}

void FakeCamera::setOpenError(std::string message) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_openError = std::move(message);
}

void FakeCamera::replug() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_plugged = true;
    m_unplugAfter = -1;
    m_cursor = 0;
    m_delivered = 0;
    m_nextDue = std::chrono::steady_clock::now();
    m_cv.notify_all();
}

int FakeCamera::framesDelivered() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_delivered;
}

} // namespace mikmap
