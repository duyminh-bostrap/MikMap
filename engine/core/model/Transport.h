// ════════════════════════════════════════════════════════════════════════
//  core/model/Transport.h — máy trạng thái phát của clip
//  (C1 C2 C3 C4 C5 C6 C7)
//
//  Đây là LOGIC THUẦN, không chạm vào decoder video. Nó chỉ tính "vị trí
//  đầu phát nên ở đâu sau dt giây". render/ đọc kết quả rồi ra lệnh cho
//  player thật seek tới đó.
//
//  Tách như vậy để:
//    · test được toàn bộ hành vi phát mà không cần file video hay GPU
//    · logic giống hệt nhau cho video, ảnh, generator, và clip rỗng
//    · sensor có thể lái đầu phát (scrub theo vị trí chạm) mà không phải
//      biết gì về ofxHapPlayer
//
//  Vị trí dùng đơn vị CHUẨN HOÁ [0,1], không phải giây — nhờ vậy đổi
//  file có độ dài khác không làm hỏng in/out point đã đặt.
// ════════════════════════════════════════════════════════════════════════
#pragma once

namespace hexmap {

enum class PlayState {
    Stopped = 0,
    Playing,
    Paused,
};

enum class PlayDirection {
    Forward = 0,
    Reverse,
    PingPong,   ///< đảo chiều mỗi khi chạm biên
};

/// Việc xảy ra khi đầu phát chạm outPoint.
enum class EndAction {
    Loop = 0,     ///< quay lại inPoint (mặc định)
    Stop,         ///< dừng tại outPoint
    HoldLast,     ///< giữ khung cuối, coi như vẫn đang phát
    PlayNext,     ///< báo cho Layer chuyển sang clip cột kế tiếp
    Random,       ///< báo cho Layer nhảy tới clip ngẫu nhiên
};

/// Cách clip phản ứng khi được kích hoạt.
enum class TriggerStyle {
    Toggle = 0,   ///< bấm để phát, bấm nữa để dừng — mặc định
    Piano,        ///< phát khi giữ, dừng khi nhả
};

/// Kết quả của một bước cập nhật — Layer đọc cái này để quyết định
/// có phải chuyển clip không.
enum class TransportEvent {
    None = 0,
    ReachedEnd,      ///< chạm outPoint, đã xử lý theo endAction nội bộ
    RequestNext,     ///< endAction == PlayNext
    RequestRandom,   ///< endAction == Random
};

struct Transport {
    PlayState     state       = PlayState::Stopped;
    PlayDirection direction   = PlayDirection::Forward;
    EndAction     endAction   = EndAction::Loop;

    double speed       = 1.0;   ///< hệ số nhân; âm cũng hợp lệ
    double inPoint     = 0.0;   ///< [0,1]
    double outPoint    = 1.0;   ///< [0,1]
    double position    = 0.0;   ///< [0,1] — vị trí đầu phát hiện tại
    double durationSec = 0.0;   ///< độ dài thật của media, 0 nếu là ảnh tĩnh

    /// Dấu chiều hiện tại của PingPong. +1 xuôi, −1 ngược.
    int pingPongSign = 1;

    // ── Điều khiển ─────────────────────────────────────────────────────
    void play();
    void pause();
    void stop();          ///< dừng VÀ đưa đầu phát về inPoint
    void togglePlay();

    /// Tiến đầu phát thêm dt giây. Đây là hàm cốt lõi.
    /// @return sự kiện để Layer xử lý (chuyển clip, v.v.)
    TransportEvent advance(double dtSec);

    /// Đặt vị trí trực tiếp (scrub). Tự kẹp vào [inPoint, outPoint].
    void seekNormalized(double t);

    // ── Truy vấn ───────────────────────────────────────────────────────
    bool   isPlaying() const { return state == PlayState::Playing; }
    double positionSec() const { return position * durationSec; }

    /// Độ dài đoạn được cắt, tính theo tỉ lệ.
    double trimmedLength() const;

    /// Đặt in/out point an toàn: tự hoán đổi nếu vào ngược, tự kẹp [0,1],
    /// và đảm bảo còn một khoảng tối thiểu để không chia cho 0.
    void setTrim(double in, double out);
};

} // namespace hexmap
