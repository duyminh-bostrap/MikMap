#include "core/model/Transport.h"

#include <algorithm>
#include <cmath>

namespace mikmap {
namespace {
/// Khoảng cắt tối thiểu — chặn chia cho 0 khi in ≈ out.
constexpr double kMinTrim = 1e-6;
} // namespace

void Transport::play() {
    state = PlayState::Playing;
}

void Transport::pause() {
    if (state == PlayState::Playing) state = PlayState::Paused;
}

void Transport::stop() {
    state = PlayState::Stopped;
    // Đưa về đầu đoạn cắt theo CHIỀU phát: clip chạy ngược thì "đầu"
    // của nó là outPoint. Nếu không xử lý, bấm stop rồi play lại sẽ
    // không phát gì vì đầu phát đã ở cuối rồi.
    position = (direction == PlayDirection::Reverse) ? outPoint : inPoint;
    pingPongSign = 1;
}

void Transport::togglePlay() {
    if (state == PlayState::Playing) pause();
    else                             play();
}

double Transport::trimmedLength() const {
    return std::max(kMinTrim, outPoint - inPoint);
}

void Transport::setTrim(double in, double out) {
    in  = std::clamp(in,  0.0, 1.0);
    out = std::clamp(out, 0.0, 1.0);
    if (in > out) std::swap(in, out);

    // Giữ một khoảng tối thiểu — người dùng kéo hai handle chồng lên nhau
    // là chuyện thường, và khoảng bằng 0 sẽ làm advance() chia cho 0.
    if (out - in < kMinTrim) {
        out = std::min(1.0, in + kMinTrim);
        if (out - in < kMinTrim) in = std::max(0.0, out - kMinTrim);
    }

    inPoint  = in;
    outPoint = out;
    position = std::clamp(position, inPoint, outPoint);
}

void Transport::seekNormalized(double t) {
    position = std::clamp(t, inPoint, outPoint);
}

TransportEvent Transport::advance(double dtSec) {
    if (state != PlayState::Playing) return TransportEvent::None;

    // Ảnh tĩnh / generator: không có độ dài, đầu phát đứng yên.
    // Vẫn coi là "đang phát" để layer render nó.
    if (durationSec <= 0.0) return TransportEvent::None;

    // Quy đổi thời gian thực sang bước chuẩn hoá.
    double step = (dtSec * speed) / durationSec;

    switch (direction) {
    case PlayDirection::Forward:                       break;
    case PlayDirection::Reverse:  step = -step;        break;
    case PlayDirection::PingPong: step *= pingPongSign; break;
    }

    position += step;

    const double lo = inPoint;
    const double hi = outPoint;

    // Chưa chạm biên — trường hợp phổ biến nhất, thoát sớm.
    if (position >= lo && position <= hi) return TransportEvent::None;

    // ── Đã chạm biên ───────────────────────────────────────────────────
    if (direction == PlayDirection::PingPong) {
        // Dội lại thay vì kẹp cứng: giữ nguyên phần dư của bước, nhờ vậy
        // tốc độ phát không bị khựng một nhịp ở mỗi lần đảo chiều.
        if (position > hi) {
            position = hi - (position - hi);
            pingPongSign = -1;
        } else {
            position = lo + (lo - position);
            pingPongSign = 1;
        }
        position = std::clamp(position, lo, hi);
        return TransportEvent::ReachedEnd;
    }

    switch (endAction) {
    case EndAction::Loop: {
        // Quấn vòng có giữ phần dư — với clip ngắn và dt lớn (ví dụ khi
        // hệ thống khựng một nhịp), kẹp cứng sẽ làm trôi nhịp tích luỹ.
        const double len = trimmedLength();
        double rel = std::fmod(position - lo, len);
        if (rel < 0.0) rel += len;
        position = lo + rel;
        return TransportEvent::ReachedEnd;
    }

    case EndAction::Stop:
        position = (step > 0.0) ? hi : lo;
        state = PlayState::Stopped;
        return TransportEvent::ReachedEnd;

    case EndAction::HoldLast:
        position = (step > 0.0) ? hi : lo;
        return TransportEvent::ReachedEnd;

    case EndAction::PlayNext:
        position = (step > 0.0) ? hi : lo;
        return TransportEvent::RequestNext;

    case EndAction::Random:
        position = (step > 0.0) ? hi : lo;
        return TransportEvent::RequestRandom;
    }

    return TransportEvent::None;
}

} // namespace mikmap
