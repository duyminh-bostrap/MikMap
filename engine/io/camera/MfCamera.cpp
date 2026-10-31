// ════════════════════════════════════════════════════════════════════════
//  io/camera/MfCamera.cpp — camera 2D qua Media Foundation
//
//  Cấu trúc:
//    · listCameras()   dò thiết bị video capture bằng MFEnumDeviceSources.
//    · MfCamera        IMFSourceReader ở chế độ ASYNC callback: ReadSample
//                      trả về ngay, callback báo khung, read() chờ có
//                      timeout. Nhờ vậy close() đánh thức được read() đang
//                      chờ và timeout là thật, không phải "chờ may rủi".
//    · Ảnh ra luôn là RGB24, thứ tự byte R,G,B. MFVideoFormat_RGB24 của MF
//      là BGR (DIB) nên đường copy nhanh đảo kênh; nếu thiết bị không nhận
//      RGB24, ta giữ loại gốc và tự chuyển NV12 / YUY2 trong read().
//
//  Khởi tạo nền tảng:
//    · COM là chuyện của TỪNG THREAD (CoInitializeEx/CoUninitialize), đếm
//      tham chiếu theo thread.
//    · MFStartup/MFShutdown đếm tham chiếu TOÀN CỤC: reader tạo ở thread
//      điều khiển nhưng đọc ở thread capture, nên không được để nền tảng
//      tắt trong khoảng giữa. Mỗi lời gọi cũng giữ một tham chiếu trong
//      suốt thời gian dùng, và MfCamera giữ thêm một tham chiếu từ open()
//      tới close().
// ════════════════════════════════════════════════════════════════════════
#include "io/camera/MfCamera.h"

#include "io/camera/MfCameraConvert.h"

#if defined(_WIN32)

#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>

#  include <mfapi.h>
#  include <mferror.h>
#  include <mfidl.h>
#  include <mfreadwrite.h>
#  include <objbase.h>
#  include <wrl/client.h>

#  include <chrono>
#  include <cstddef>
#  include <cstdio>
#  include <cstdlib>
#  include <memory>
#  include <mutex>
#  include <utility>

namespace mikmap {
namespace {

using Microsoft::WRL::ComPtr;

// MF_SOURCE_READER_* là enum có giá trị âm (0xFFFFFFFC…) nên phải ép kiểu
// tường minh sang DWORD; thiếu nó MSVC /W4 cảnh báo C4245.
constexpr DWORD kFirstVideoStream = static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM);
constexpr DWORD kAllStreams = static_cast<DWORD>(MF_SOURCE_READER_ALL_STREAMS);

// ────────────────────────────────────────────────────────────────────────
//  Tiện ích
// ────────────────────────────────────────────────────────────────────────

std::string hrText(HRESULT hr) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "0x%08lX", static_cast<unsigned long>(hr));
    return std::string(buf);
}

std::string utf8(const wchar_t* w) {
    if (!w || !*w) return {};
    const int n = ::WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    if (n <= 1) return {};
    std::string s(static_cast<size_t>(n - 1), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, w, -1, s.data(), n, nullptr, nullptr);
    return s;
}

std::string attrString(IMFAttributes* attrs, REFGUID key) {
    if (!attrs) return {};
    LPWSTR value = nullptr;
    UINT32 len = 0;
    if (FAILED(attrs->GetAllocatedString(key, &value, &len)) || !value) return {};
    std::string s = utf8(value);
    ::CoTaskMemFree(value);
    return s;
}

int64_t steadyNs() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

// ────────────────────────────────────────────────────────────────────────
//  Khởi tạo COM (theo thread) + Media Foundation (toàn cục)
// ────────────────────────────────────────────────────────────────────────

struct ThreadCom {
    int refs = 0;
    bool ownsInit = false;

    bool acquire(std::string& err) {
        if (refs > 0) {
            ++refs;
            return true;
        }
        const HRESULT hr = ::CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (hr == RPC_E_CHANGED_MODE) {
            // Thread đã khởi tạo COM ở apartment khác (UI): dùng luôn nhưng
            // KHÔNG CoUninitialize — ta không phải người khởi tạo.
            ownsInit = false;
        } else if (FAILED(hr)) {
            err = "Media Foundation: COM init failed (" + hrText(hr) + ")";
            return false;
        } else {
            ownsInit = true;
        }
        refs = 1;
        return true;
    }

    void release() {
        if (refs <= 0) return;
        if (--refs > 0) return;
        if (ownsInit) {
            ::CoUninitialize();
            ownsInit = false;
        }
    }

    ~ThreadCom() {
        while (refs > 0) release();
    }
};

ThreadCom& threadCom() {
    thread_local ThreadCom com;
    return com;
}

std::mutex g_mfMutex;
int g_mfRefs = 0;

bool mfStart(std::string& err) {
    std::lock_guard<std::mutex> lock(g_mfMutex);
    if (g_mfRefs == 0) {
        const HRESULT hr = ::MFStartup(MF_VERSION, MFSTARTUP_FULL);
        if (FAILED(hr)) {
            err = "Media Foundation: MFStartup failed (" + hrText(hr) + ")";
            return false;
        }
    }
    ++g_mfRefs;
    return true;
}

void mfStop() {
    std::lock_guard<std::mutex> lock(g_mfMutex);
    if (g_mfRefs > 0 && --g_mfRefs == 0) {
        ::MFShutdown();
    }
}

/// Một lời gọi vào MF: giữ COM của thread + một tham chiếu MF toàn cục.
class MfGuard {
public:
    explicit MfGuard(std::string& err) {
        if (!threadCom().acquire(err)) return;
        if (!mfStart(err)) {
            threadCom().release();
            return;
        }
        m_ok = true;
    }

    ~MfGuard() {
        if (m_ok) {
            mfStop();
            threadCom().release();
        }
    }

    MfGuard(const MfGuard&) = delete;
    MfGuard& operator=(const MfGuard&) = delete;

    bool ok() const { return m_ok; }

private:
    bool m_ok = false;
};

// ────────────────────────────────────────────────────────────────────────
//  Dò thiết bị
// ────────────────────────────────────────────────────────────────────────

struct DeviceEntry {
    ComPtr<IMFActivate> activate;
    std::string id;
    std::string name;
};

bool enumDevices(std::vector<DeviceEntry>& out, std::string& err) {
    out.clear();

    ComPtr<IMFAttributes> attrs;
    HRESULT hr = ::MFCreateAttributes(&attrs, 1);
    if (FAILED(hr)) {
        err = "Media Foundation: cannot create attributes (" + hrText(hr) + ")";
        return false;
    }
    hr = attrs->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
                        MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
    if (FAILED(hr)) {
        err = "Media Foundation: cannot set device type (" + hrText(hr) + ")";
        return false;
    }

    IMFActivate** devices = nullptr;
    UINT32 count = 0;
    hr = ::MFEnumDeviceSources(attrs.Get(), &devices, &count);
    if (FAILED(hr)) {
        err = "Media Foundation: cannot enumerate cameras (" + hrText(hr) + ")";
        return false;
    }

    out.reserve(count);
    for (UINT32 i = 0; i < count; ++i) {
        DeviceEntry entry;
        entry.activate.Attach(devices[i]);   // nhận quyền sở hữu tham chiếu
        entry.id = attrString(devices[i], MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK);
        entry.name = attrString(devices[i], MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME);
        if (entry.id.empty()) entry.id = "camera:" + std::to_string(i);
        if (entry.name.empty()) entry.name = "Camera " + std::to_string(i);
        out.push_back(std::move(entry));
    }
    // Chỉ mảng do MFEnumDeviceSources cấp phát là của ta; từng phần tử đã
    // được ComPtr nhận ở trên.
    ::CoTaskMemFree(devices);
    return true;
}

bool parseIndex(const std::string& s, size_t& out) {
    if (s.empty()) return false;
    size_t v = 0;
    for (const char ch : s) {
        if (ch < '0' || ch > '9') return false;
        v = v * 10 + static_cast<size_t>(ch - '0');
        if (v > 100000) return false;
    }
    out = v;
    return true;
}

bool isBusyHr(HRESULT hr) {
    return hr == MF_E_VIDEO_DEVICE_LOCKED ||
           hr == MF_E_VIDEO_RECORDING_DEVICE_PREEMPTED ||
           hr == MF_E_HW_MFT_FAILED_START_STREAMING ||
           hr == E_ACCESSDENIED ||
           hr == HRESULT_FROM_WIN32(ERROR_BUSY) ||
           hr == HRESULT_FROM_WIN32(ERROR_SHARING_VIOLATION);
}

bool isGoneHr(HRESULT hr) {
    return hr == MF_E_VIDEO_RECORDING_DEVICE_INVALIDATED ||
           hr == MF_E_NOT_FOUND ||
           hr == HRESULT_FROM_WIN32(ERROR_DEVICE_NOT_CONNECTED) ||
           hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
}

std::string deviceOpenError(HRESULT hr, const std::string& name) {
    if (isBusyHr(hr)) {
        return "Camera \"" + name + "\" is in use by another application (" + hrText(hr) + ")";
    }
    if (isGoneHr(hr)) {
        return "Camera \"" + name + "\" is gone (" + hrText(hr) + ")";
    }
    return "Camera \"" + name + "\": " + hrText(hr);
}

/// Lỗi đến muộn, ở lần ReadSample đầu tiên (streaming khởi động lười) hoặc
/// giữa chừng. Cùng cách gọi tên như lúc mở để UI không phải đoán.
std::string streamError(HRESULT hr, const std::string& name) {
    if (isBusyHr(hr)) {
        return "Camera \"" + name + "\" is in use by another application (" + hrText(hr) + ")";
    }
    if (isGoneHr(hr)) {
        return "Camera \"" + name + "\" is gone (" + hrText(hr) + ")";
    }
    return "Camera \"" + name + "\": read failed (" + hrText(hr) + ")";
}

// ────────────────────────────────────────────────────────────────────────
//  Chọn cỡ ảnh
// ────────────────────────────────────────────────────────────────────────

struct ModeInfo {
    int width = 0;
    int height = 0;
    UINT32 fpsNum = 0;
    UINT32 fpsDen = 0;
    DWORD typeIndex = 0;
};

std::vector<ModeInfo> nativeModes(IMFSourceReader* reader) {
    std::vector<ModeInfo> modes;
    for (DWORD i = 0;; ++i) {
        ComPtr<IMFMediaType> type;
        if (FAILED(reader->GetNativeMediaType(kFirstVideoStream, i, &type)) ||
            !type) {
            break;
        }
        UINT32 w = 0, h = 0;
        if (FAILED(::MFGetAttributeSize(type.Get(), MF_MT_FRAME_SIZE, &w, &h)) || w == 0 || h == 0) {
            continue;
        }
        ModeInfo m;
        m.width = static_cast<int>(w);
        m.height = static_cast<int>(h);
        m.typeIndex = i;
        UINT32 num = 0, den = 0;
        if (SUCCEEDED(::MFGetAttributeRatio(type.Get(), MF_MT_FRAME_RATE, &num, &den)) && den != 0) {
            m.fpsNum = num;
            m.fpsDen = den;
        }
        modes.push_back(m);
    }
    return modes;
}

/// Cỡ nhỏ nhất vẫn ≥ yêu cầu; không có thì cỡ lớn nhất. width/height ≤ 0
/// nghĩa là không có yêu cầu → cỡ lớn nhất.
bool pickMode(const std::vector<ModeInfo>& modes, int wantW, int wantH, ModeInfo& out) {
    if (modes.empty()) return false;

    const bool anySize = wantW <= 0 || wantH <= 0;
    const ModeInfo* best = nullptr;
    int64_t bestArea = 0;

    if (!anySize) {
        for (const ModeInfo& m : modes) {
            if (m.width < wantW || m.height < wantH) continue;
            const int64_t area = static_cast<int64_t>(m.width) * m.height;
            const int dist = std::abs(m.width - wantW) + std::abs(m.height - wantH);
            const int bestDist =
                best ? std::abs(best->width - wantW) + std::abs(best->height - wantH) : 0;
            if (!best || area < bestArea || (area == bestArea && dist < bestDist)) {
                best = &m;
                bestArea = area;
            }
        }
    }

    if (!best) {
        for (const ModeInfo& m : modes) {
            const int64_t area = static_cast<int64_t>(m.width) * m.height;
            if (!best || area > bestArea) {
                best = &m;
                bestArea = area;
            }
        }
    }

    if (!best) return false;
    out = *best;
    return true;
}

// ────────────────────────────────────────────────────────────────────────
//  Callback của source reader (async)
//
//  Callback chỉ chạm SampleState (shared_ptr), không chạm MfCamera::Impl —
//  nhờ vậy một callback đến muộn sau khi close() cũng không thể đọc vùng
//  nhớ đã giải phóng.
// ────────────────────────────────────────────────────────────────────────

struct SampleState {
    std::mutex mutex;
    std::condition_variable cv;
    ComPtr<IMFSample> sample;
    HRESULT hr = S_OK;
    DWORD flags = 0;
    bool frameReady = false;
    bool requestPending = false;
    bool stopping = false;
    bool dead = false;
    bool failed = false;          ///< lỗi dính: reader chết thì đừng gọi lại
    std::string failure;
    int callbacksInFlight = 0;
};

class SampleCallback final : public IMFSourceReaderCallback {
public:
    explicit SampleCallback(std::shared_ptr<SampleState> state) : m_state(std::move(state)) {}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (riid == __uuidof(IUnknown) || riid == __uuidof(IMFSourceReaderCallback)) {
            *ppv = static_cast<IMFSourceReaderCallback*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override {
        return static_cast<ULONG>(::InterlockedIncrement(&m_ref));
    }

    ULONG STDMETHODCALLTYPE Release() override {
        const ULONG n = static_cast<ULONG>(::InterlockedDecrement(&m_ref));
        if (n == 0) delete this;
        return n;
    }

    HRESULT STDMETHODCALLTYPE OnReadSample(HRESULT hrStatus, DWORD streamIndex, DWORD streamFlags,
                                           LONGLONG timestamp, IMFSample* sample) override {
        (void)streamIndex;
        (void)timestamp;
        {
            std::lock_guard<std::mutex> lock(m_state->mutex);
            ++m_state->callbacksInFlight;
            m_state->requestPending = false;
            if (!m_state->stopping && !m_state->dead) {
                m_state->hr = hrStatus;
                m_state->flags = streamFlags;
                m_state->sample = sample;   // ComPtr copy; nullptr được
                m_state->frameReady = true;
            }
        }
        m_state->cv.notify_all();
        {
            std::lock_guard<std::mutex> lock(m_state->mutex);
            --m_state->callbacksInFlight;
        }
        m_state->cv.notify_all();
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnFlush(DWORD streamIndex) override {
        (void)streamIndex;
        {
            std::lock_guard<std::mutex> lock(m_state->mutex);
            m_state->requestPending = false;
        }
        m_state->cv.notify_all();
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnEvent(DWORD streamIndex, IMFMediaEvent* event) override {
        (void)streamIndex;
        (void)event;
        return S_OK;
    }

private:
    ~SampleCallback() = default;

    LONG m_ref = 1;
    std::shared_ptr<SampleState> m_state;
};

} // namespace

// ────────────────────────────────────────────────────────────────────────
//  MfCamera::Impl
// ────────────────────────────────────────────────────────────────────────

struct MfCamera::Impl {
    mutable std::mutex mutex;
    std::condition_variable cv;      ///< read() ngủ ở đây; close() đánh thức
    std::mutex readerMutex;          ///< tuần tự hoá lời gọi lên reader

    std::shared_ptr<SampleState> state;
    ComPtr<IMFSourceReader> reader;
    ComPtr<IMFMediaType> mediaType;
    ComPtr<SampleCallback> callback;

    CameraCaps caps;
    std::string name;
    GUID subtype{};
    LONG stride = 0;
    int width = 0;
    int height = 0;
    uint64_t frameIndex = 0;
    int64_t lastCaptureNs = 0;
    bool open = false;
    bool readActive = false;
    bool mfHeld = false;             ///< giữ MFStartup từ open() tới close()

    void close();
    bool read(CameraFrame& out, int timeoutMs, std::string& err);
    bool convert(IMFSample* sample, CameraFrame& out, std::string& err) const;
};

bool MfCamera::Impl::convert(IMFSample* sample, CameraFrame& out, std::string& err) const {
    ComPtr<IMFMediaBuffer> buffer;
    HRESULT hr = sample->GetBufferByIndex(0, &buffer);
    if (FAILED(hr) || !buffer) {
        err = "Camera: no sample buffer (" + hrText(hr) + ")";
        return false;
    }

    const size_t rowBytes = static_cast<size_t>(width) * 3u;
    const size_t need = rowBytes * static_cast<size_t>(height);
    if (out.rgb.size() != need) out.rgb.resize(need);

    // Ưu tiên IMF2DBuffer: pitch do buffer khai, không phải giả định w*3.
    ComPtr<IMF2DBuffer> buffer2d;
    BYTE* data = nullptr;
    LONG pitch = 0;
    bool locked2d = false;
    DWORD curLen = 0;
    if (SUCCEEDED(buffer.As(&buffer2d)) && buffer2d &&
        SUCCEEDED(buffer2d->Lock2D(&data, &pitch)) && data) {
        locked2d = true;
    } else {
        DWORD maxLen = 0;
        hr = buffer->Lock(&data, &maxLen, &curLen);
        if (FAILED(hr) || !data) {
            err = "Camera: cannot lock sample buffer (" + hrText(hr) + ")";
            return false;
        }
        pitch = stride;
    }

    LONG fallbackPitch = static_cast<LONG>(rowBytes);
    if (IsEqualGUID(subtype, MFVideoFormat_NV12)) {
        fallbackPitch = static_cast<LONG>(width);
    } else if (IsEqualGUID(subtype, MFVideoFormat_YUY2)) {
        fallbackPitch = static_cast<LONG>(width) * 2;
    }
    if (pitch == 0) pitch = fallbackPitch;

    // Kích thước nguồn phụ thuộc định dạng: NV12 chỉ có w*h*1.5 byte, không
    // phải w*h*3 như ảnh RGB đích — kiểm tra theo nguồn, nếu không sẽ từ
    // chối nhầm mọi khung NV12 hợp lệ.
    if (!locked2d) {
        const size_t absPitch = static_cast<size_t>(pitch >= 0 ? pitch : -pitch);
        size_t srcNeed = 0;
        if (IsEqualGUID(subtype, MFVideoFormat_RGB24)) {
            srcNeed = absPitch * static_cast<size_t>(height - 1) + rowBytes;
        } else if (IsEqualGUID(subtype, MFVideoFormat_NV12)) {
            srcNeed = absPitch * static_cast<size_t>(height) * 3u / 2u;
        } else if (IsEqualGUID(subtype, MFVideoFormat_YUY2)) {
            srcNeed = absPitch * static_cast<size_t>(height);
        }
        if (srcNeed != 0 && curLen < srcNeed) {
            buffer->Unlock();
            err = "Camera: short sample buffer";
            return false;
        }
    }

    bool ok = true;
    if (IsEqualGUID(subtype, MFVideoFormat_RGB24)) {
        camera_detail::bgr24ToRgb(data, pitch, width, height, out.rgb.data());
    } else if (IsEqualGUID(subtype, MFVideoFormat_NV12)) {
        if (pitch < 0) {
            err = "Camera: unsupported negative stride for NV12";
            ok = false;
        } else {
            camera_detail::nv12ToRgb(data, pitch, width, height, out.rgb.data());
        }
    } else if (IsEqualGUID(subtype, MFVideoFormat_YUY2)) {
        camera_detail::yuy2ToRgb(data, pitch, width, height, out.rgb.data());
    } else {
        err = "Camera: unsupported pixel format";
        ok = false;
    }

    if (locked2d) {
        buffer2d->Unlock2D();
    } else {
        buffer->Unlock();
    }
    return ok;
}

bool MfCamera::Impl::read(CameraFrame& out, int timeoutMs, std::string& err) {
    err.clear();

    std::shared_ptr<SampleState> st;
    ComPtr<IMFSourceReader> rd;
    std::string camName;
    int w = 0, h = 0;
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (!open || !state || !reader) {
            err = "Camera is not open";
            return false;
        }
        st = state;
        rd = reader;
        camName = name;
        w = width;
        h = height;
        readActive = true;
    }

    // Bất kể read() thoát ở đâu, close() phải thấy readActive = false.
    struct ReadFlag {
        Impl& impl;
        ~ReadFlag() {
            {
                std::lock_guard<std::mutex> lock(impl.mutex);
                impl.readActive = false;
            }
            impl.cv.notify_all();
        }
    } flag{*this};

    // Chỉ đặt ReadSample khi chưa có yêu cầu nào đang chờ. Một lần timeout
    // để yêu cầu cũ treo lại; lần đọc sau chỉ việc chờ tiếp — không đặt
    // chồng hai yêu cầu lên source reader.
    bool needRequest = false;
    {
        std::lock_guard<std::mutex> lock(st->mutex);
        if (st->stopping) {
            err = "Camera is closed";
            return false;
        }
        if (st->failed) {
            err = st->failure;
            return false;
        }
        if (!st->requestPending && !st->frameReady) {
            st->requestPending = true;
            needRequest = true;
        }
    }

    if (needRequest) {
        const HRESULT hr =
            rd->ReadSample(kFirstVideoStream, 0, nullptr, nullptr, nullptr, nullptr);
        if (FAILED(hr)) {
            std::lock_guard<std::mutex> lock(st->mutex);
            st->requestPending = false;
            st->failed = true;
            st->failure = streamError(hr, camName);
            err = st->failure;
            return false;
        }
    }

    ComPtr<IMFSample> sample;
    HRESULT sampleHr = S_OK;
    DWORD flags = 0;
    {
        std::unique_lock<std::mutex> lock(st->mutex);
        st->cv.wait_for(lock, std::chrono::milliseconds(timeoutMs > 0 ? timeoutMs : 0),
                        [&st] { return st->frameReady || st->stopping; });
        if (st->stopping) {
            err = "Camera is closed";
            return false;
        }
        if (!st->frameReady) {
            // Yêu cầu vẫn treo; khung đến muộn sẽ được lần đọc sau nhận.
            err = "Camera: no frame within timeout";
            return false;
        }
        st->frameReady = false;
        sampleHr = st->hr;
        flags = st->flags;
        sample = st->sample;
        st->sample.Reset();
    }

    std::string failure;
    if (flags & MF_SOURCE_READERF_ENDOFSTREAM) {
        failure = "Camera \"" + camName + "\": stream ended";
    } else if (FAILED(sampleHr)) {
        // hrStatus cụ thể hơn flags: giữ được câu "in use by another
        // application" thay vì một lỗi chung chung.
        failure = streamError(sampleHr, camName);
    } else if (flags & MF_SOURCE_READERF_ERROR) {
        failure = "Camera \"" + camName + "\": stream error";
    } else if (!sample) {
        failure = "Camera \"" + camName + "\": no sample";
    }
    if (!failure.empty()) {
        // Reader đã chết: ghi nhớ để lần đọc sau báo lỗi ngay, không treo.
        {
            std::lock_guard<std::mutex> lock(st->mutex);
            st->failed = true;
            st->failure = failure;
        }
        err = failure;
        return false;
    }

    out.width = w;
    out.height = h;
    out.depthW = 0;
    out.depthH = 0;
    out.depthMm.clear();

    if (!convert(sample.Get(), out, err)) return false;

    // Mốc thời gian lấy ngay khi khung được nhận; kẹp để luôn tăng thật sự
    // kể cả khi đồng hồ độ phân giải thấp trả về cùng một giá trị.
    const int64_t now = steadyNs();
    out.tCaptureNs = now > lastCaptureNs ? now : lastCaptureNs + 1;
    lastCaptureNs = out.tCaptureNs;
    out.index = frameIndex++;
    return true;
}

void MfCamera::Impl::close() {
    std::shared_ptr<SampleState> st;
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (!open && !reader && !state) return;
        open = false;
        st = state;
    }

    if (st) {
        {
            std::lock_guard<std::mutex> lock(st->mutex);
            st->stopping = true;
            st->frameReady = false;
            st->sample.Reset();
        }
        st->cv.notify_all();
    }

    // Chờ read() rời reader trước khi giải phóng nó.
    {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait_for(lock, std::chrono::milliseconds(2000), [this] { return !readActive; });
    }

    // Huỷ yêu cầu ReadSample đang treo (async: Flush trả về ngay).
    {
        std::lock_guard<std::mutex> lock(readerMutex);
        if (reader) reader->Flush(kAllStreams);
    }

    // Đợi callback đang chạy xong rồi mới thả reader (reader giữ một tham
    // chiếu tới callback).
    if (st) {
        std::unique_lock<std::mutex> lock(st->mutex);
        st->cv.wait_for(lock, std::chrono::milliseconds(2000),
                        [&st] { return st->callbacksInFlight == 0; });
        st->dead = true;
    }

    {
        std::lock_guard<std::mutex> lock(readerMutex);
        reader.Reset();
        mediaType.Reset();
    }
    callback.Reset();

    {
        std::lock_guard<std::mutex> lock(mutex);
        state.reset();
        width = 0;
        height = 0;
        name.clear();
        subtype = GUID{};
        stride = 0;
        caps = CameraCaps{};
        if (mfHeld) {
            mfStop();
            mfHeld = false;
        }
    }
}

// ────────────────────────────────────────────────────────────────────────
//  MfCamera
// ────────────────────────────────────────────────────────────────────────

MfCamera::MfCamera() : m_impl(std::make_unique<Impl>()) {}

MfCamera::~MfCamera() {
    close();
}

bool MfCamera::open(const std::string& id, int width, int height, std::string& err) {
    err.clear();
    close();

    std::string ctxErr;
    MfGuard guard(ctxErr);
    if (!guard.ok()) {
        err = ctxErr;
        return false;
    }

    std::vector<DeviceEntry> devices;
    if (!enumDevices(devices, err)) return false;

    size_t index = 0;
    bool found = false;
    if (id.empty()) {
        if (!devices.empty()) {
            found = true;
        } else {
            err = "No camera found";
        }
    } else {
        size_t n = 0;
        if (parseIndex(id, n)) {
            if (n < devices.size()) {
                index = n;
                found = true;
            }
        } else {
            for (size_t i = 0; i < devices.size(); ++i) {
                if (devices[i].id == id) {
                    index = i;
                    found = true;
                    break;
                }
            }
        }
        if (!found) err = "Camera not found: " + id;
    }
    if (!found) return false;

    const std::string& name = devices[index].name;

    ComPtr<IMFMediaSource> source;
    HRESULT hr = devices[index].activate->ActivateObject(IID_PPV_ARGS(&source));
    if (FAILED(hr) || !source) {
        err = deviceOpenError(hr, name);
        return false;
    }

    auto state = std::make_shared<SampleState>();
    ComPtr<SampleCallback> callback;
    callback.Attach(new SampleCallback(state));

    ComPtr<IMFAttributes> attrs;
    hr = ::MFCreateAttributes(&attrs, 2);
    if (FAILED(hr)) {
        err = "Media Foundation: cannot create reader attributes (" + hrText(hr) + ")";
        return false;
    }
    hr = attrs->SetUnknown(MF_SOURCE_READER_ASYNC_CALLBACK, static_cast<IUnknown*>(callback.Get()));
    if (FAILED(hr)) {
        // Không có async callback thì read() không có timeout thật — dừng.
        err = "Media Foundation: cannot enable async reader (" + hrText(hr) + ")";
        return false;
    }
    // Tuỳ chọn: cho source reader dùng video processor MFT để trả thẳng
    // RGB24. Nếu máy không có MFT này, open() vẫn chạy bằng loại gốc.
    attrs->SetUINT32(MF_SOURCE_READER_ENABLE_ADVANCED_VIDEO_PROCESSING, TRUE);

    ComPtr<IMFSourceReader> reader;
    hr = ::MFCreateSourceReaderFromMediaSource(source.Get(), attrs.Get(), &reader);
    if (FAILED(hr)) {
        // Một số driver từ chối thuộc tính advanced video processing; thử
        // lại ở chế độ thường trước khi báo lỗi.
        attrs->DeleteItem(MF_SOURCE_READER_ENABLE_ADVANCED_VIDEO_PROCESSING);
        hr = ::MFCreateSourceReaderFromMediaSource(source.Get(), attrs.Get(), &reader);
    }
    if (FAILED(hr) || !reader) {
        err = deviceOpenError(hr, name);
        return false;
    }

    // Chỉ dùng luồng video đầu tiên (webcam chỉ có một).
    hr = reader->SetStreamSelection(kAllStreams, FALSE);
    if (SUCCEEDED(hr)) {
        hr = reader->SetStreamSelection(kFirstVideoStream, TRUE);
    }
    if (FAILED(hr)) {
        err = "Camera \"" + name + "\": cannot select video stream (" + hrText(hr) + ")";
        return false;
    }

    const std::vector<ModeInfo> modes = nativeModes(reader.Get());
    ModeInfo chosen;
    if (!pickMode(modes, width, height, chosen)) {
        err = "Camera \"" + name + "\" has no video modes";
        return false;
    }

    // Yêu cầu RGB24 ở cỡ đã chọn. Source reader tự chuyển bằng video
    // processor MFT (NV12/YUY2/MJPG → RGB24).
    ComPtr<IMFMediaType> outType;
    hr = ::MFCreateMediaType(&outType);
    if (FAILED(hr)) {
        err = "Media Foundation: cannot create media type (" + hrText(hr) + ")";
        return false;
    }
    outType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    outType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB24);
    ::MFSetAttributeSize(outType.Get(), MF_MT_FRAME_SIZE, static_cast<UINT32>(chosen.width),
                         static_cast<UINT32>(chosen.height));
    if (chosen.fpsDen != 0) {
        ::MFSetAttributeRatio(outType.Get(), MF_MT_FRAME_RATE, chosen.fpsNum, chosen.fpsDen);
    }
    ::MFSetAttributeRatio(outType.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
    outType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    outType->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);

    hr = reader->SetCurrentMediaType(kFirstVideoStream, nullptr, outType.Get());
    if (FAILED(hr)) {
        // Thiết bị không nhận RGB24: dùng loại gốc, read() tự chuyển.
        ComPtr<IMFMediaType> native;
        hr = reader->GetNativeMediaType(kFirstVideoStream, chosen.typeIndex,
                                        &native);
        if (SUCCEEDED(hr) && native) {
            hr = reader->SetCurrentMediaType(kFirstVideoStream, nullptr,
                                             native.Get());
        }
        if (FAILED(hr)) {
            err = "Camera \"" + name + "\": cannot set video format (" + hrText(hr) + ")";
            return false;
        }
    }

    ComPtr<IMFMediaType> current;
    hr = reader->GetCurrentMediaType(kFirstVideoStream, &current);
    if (FAILED(hr) || !current) {
        err = "Camera \"" + name + "\": cannot read video format (" + hrText(hr) + ")";
        return false;
    }

    UINT32 curW = 0, curH = 0;
    if (FAILED(::MFGetAttributeSize(current.Get(), MF_MT_FRAME_SIZE, &curW, &curH)) || curW == 0 ||
        curH == 0) {
        err = "Camera \"" + name + "\": video format has no frame size";
        return false;
    }

    GUID subtype{};
    if (FAILED(current->GetGUID(MF_MT_SUBTYPE, &subtype))) {
        err = "Camera \"" + name + "\": video format has no subtype";
        return false;
    }
    if (!IsEqualGUID(subtype, MFVideoFormat_RGB24) && !IsEqualGUID(subtype, MFVideoFormat_NV12) &&
        !IsEqualGUID(subtype, MFVideoFormat_YUY2)) {
        err = "Camera \"" + name + "\": unsupported pixel format";
        return false;
    }

    LONG stride = static_cast<LONG>(curW) * 3;
    UINT32 strideAttr = 0;
    if (SUCCEEDED(current->GetUINT32(MF_MT_DEFAULT_STRIDE, &strideAttr)) && strideAttr != 0) {
        stride = static_cast<LONG>(static_cast<int32_t>(strideAttr));
    }

    UINT32 fpsNum = chosen.fpsNum, fpsDen = chosen.fpsDen;
    UINT32 num = 0, den = 0;
    if (SUCCEEDED(::MFGetAttributeRatio(current.Get(), MF_MT_FRAME_RATE, &num, &den)) && den != 0) {
        fpsNum = num;
        fpsDen = den;
    }
    const float fps =
        fpsDen != 0 ? static_cast<float>(fpsNum) / static_cast<float>(fpsDen) : 0.0f;

    // Giữ MF sống suốt thời gian reader tồn tại, không chỉ trong open().
    if (!mfStart(err)) return false;

    {
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        m_impl->state = std::move(state);
        m_impl->callback = std::move(callback);
        m_impl->reader = std::move(reader);
        m_impl->mediaType = std::move(current);
        m_impl->width = static_cast<int>(curW);
        m_impl->height = static_cast<int>(curH);
        m_impl->name = name;
        m_impl->subtype = subtype;
        m_impl->stride = stride;
        m_impl->caps = CameraCaps{};
        m_impl->caps.hasColor = true;
        m_impl->caps.color = StreamInfo{static_cast<int>(curW), static_cast<int>(curH), fps};
        m_impl->frameIndex = 0;
        m_impl->lastCaptureNs = 0;
        m_impl->open = true;
        m_impl->mfHeld = true;
    }
    return true;
}

void MfCamera::close() {
    if (!m_impl) return;

    // close() có thể chạy trên thread chưa từng khởi tạo COM (thread điều
    // khiển đã đóng open() từ lâu) — giữ guard cho lời gọi Flush/Release.
    std::string ignored;
    MfGuard guard(ignored);
    m_impl->close();
}

CameraCaps MfCamera::caps() const {
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    return m_impl->caps;
}

bool MfCamera::read(CameraFrame& out, int timeoutMs, std::string& err) {
    std::string ctxErr;
    MfGuard guard(ctxErr);
    if (!guard.ok()) {
        err = ctxErr;
        return false;
    }
    return m_impl->read(out, timeoutMs, err);
}

// ────────────────────────────────────────────────────────────────────────
//  Dò thiết bị
// ────────────────────────────────────────────────────────────────────────

std::vector<CameraInfo> listCameras(std::string& err) {
    err.clear();
    std::vector<CameraInfo> result;

    std::string ctxErr;
    MfGuard guard(ctxErr);
    if (!guard.ok()) {
        err = ctxErr;
        return result;
    }

    std::vector<DeviceEntry> devices;
    if (!enumDevices(devices, err)) return result;

    result.reserve(devices.size());
    for (const DeviceEntry& d : devices) {
        result.push_back(CameraInfo{d.id, d.name});
    }
    return result;
}

} // namespace mikmap

#else   // !_WIN32 — vẫn phải biên dịch được, mọi thứ báo "unsupported"

namespace mikmap {

struct MfCamera::Impl {};

std::vector<CameraInfo> listCameras(std::string& err) {
    err = "camera enumeration is unsupported on this platform";
    return {};
}

MfCamera::MfCamera() : m_impl(std::make_unique<Impl>()) {}

MfCamera::~MfCamera() = default;

bool MfCamera::open(const std::string& id, int width, int height, std::string& err) {
    (void)id;
    (void)width;
    (void)height;
    err = "camera capture is unsupported on this platform";
    return false;
}

void MfCamera::close() {}

CameraCaps MfCamera::caps() const {
    return CameraCaps{};
}

bool MfCamera::read(CameraFrame& out, int timeoutMs, std::string& err) {
    (void)out;
    (void)timeoutMs;
    err = "camera capture is unsupported on this platform";
    return false;
}

} // namespace mikmap

#endif   // _WIN32
