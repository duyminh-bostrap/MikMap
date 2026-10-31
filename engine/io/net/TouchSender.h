// ════════════════════════════════════════════════════════════════════════
//  io/net/TouchSender.h — send mapped touches OUT as OSC or plain UDP text
//
//  The render thread builds an OutPacket (POD, fixed size, no heap) and
//  push()es it; ONLY the sender's own thread resolves the host and calls
//  sendto(). push() never blocks, never allocates, never touches DNS:
//  a full queue drops its OLDEST packet (fresh coordinates matter more).
//  One OutPacket may leave as several datagrams of at most kMaxDatagram bytes.
//
//  Wire formats (x, y are whatever the caller put in — MikMap sends 0..1 of the screen, unclamped):
//    OSC  one or more bundles (timetag "immediately"), each at most kMaxDatagram bytes:
//           first bundle:  /mikmap/<device>/frame  i:count
//                          /mikmap/<device>/touch  i:id f:x f:y   (one message per point)
//           later bundles: only /mikmap/<device>/touch messages
//    text one or more datagrams, each at most kMaxDatagram bytes:
//           first:  "<device> frame <count>\n" then "<device> <id> <x> <y>\n" per point, x/y with 4 decimals
//           later:  only "<device> <id> <x> <y>\n" lines
//  A frame with more points than fit in one datagram is therefore split; the receiver
//  concatenates the points of the bundles in order. <device> is OutPacket::device as
//  given; setOutDevice() makes any name safe for both formats.
// ════════════════════════════════════════════════════════════════════════
#pragma once

#include "io/SensorFrame.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>
#include <thread>

namespace mikmap {

/// One UDP datagram never exceeds this: the safe payload of a single packet on
/// a LAN (jumbo frames and IP fragmentation are not assumed).
inline constexpr size_t kMaxDatagram = 1200;

struct OutPacket {                                   // POD, memcpy-able
    char     host[64] = {};
    uint16_t port = 0;                               // 0 = invalid: the sender reports it and drops the packet
    uint8_t  proto = 0;                              // 0 OSC, 1 text
    char     device[32] = {};
    uint16_t count = 0;                              // up to kMaxTouchPoints (≥ 8 bit: 512 points)
    uint32_t ids[kMaxTouchPoints] = {};
    float    xy[kMaxTouchPoints * 2] = {};
};

/// Copies `name` into p.device: every char outside [A-Za-z0-9._:-] becomes '_' (OSC address
/// specials, spaces, UTF-8 bytes), truncated to 31 bytes; empty -> "device". No allocation.
void setOutDevice(OutPacket& p, const char* name);

/// Encodes points [first, count) of `p` into ONE datagram, never above `cap` bytes.
/// The chunk starting at 0 carries the "/frame i:count" message; later chunks carry only
/// points. Sets `next` to the index of the first point not written (== count when done).
/// Returns the bytes written, or 0 when not even one point fits in `cap` (or `first` is
/// past the end); an empty frame (count == 0, first == 0) still writes the frame message.
size_t encodeOscChunk(const OutPacket& p, int first, uint8_t* buf, size_t cap, int& next);
size_t encodeTextChunk(const OutPacket& p, int first, uint8_t* buf, size_t cap, int& next);

size_t encodeOsc(const OutPacket& p, uint8_t* buf, size_t cap);    // 0 if the whole frame does not fit in cap
size_t encodeText(const OutPacket& p, uint8_t* buf, size_t cap);   // 0 if the whole frame does not fit in cap

class TouchSender {
public:
    TouchSender();
    ~TouchSender() { stop(); }
    TouchSender(const TouchSender&) = delete;
    TouchSender& operator=(const TouchSender&) = delete;

    bool start();              // idempotent; false only if the thread could not be created
    void stop();               // joins; packets still queued count as dropped
    bool running() const { return m_running.load(std::memory_order_acquire); }

    /// Producer side — exactly ONE thread (the render thread) may call this.
    /// Always returns true: when the queue is full the oldest packet is dropped instead.
    bool push(const OutPacket& p);

    uint64_t dropped() const { return m_dropped.load(std::memory_order_relaxed); }   // queue overflow + send/resolve failures
    uint64_t sent() const { return m_sent.load(std::memory_order_relaxed); }         // packets sent (all their datagrams)
    uint64_t lookups() const { return m_lookups.load(std::memory_order_relaxed); }   // name lookups (DNS) done so far; an IP literal never needs one
    std::string lastError() const;   // "" while the last failing target has since sent fine; lock-free

private:
    static constexpr size_t kCap = 64;   // power of two
    struct Cell { std::atomic<size_t> seq{0}; OutPacket data; };

    bool tryEnqueue(const OutPacket& p);
    bool tryDequeue(OutPacket* out);     // out == nullptr: discard (the producer dropping the oldest)
    void threadLoop();

    // Vyukov bounded queue: one producer, two consumers (the sender thread, and the producer
    // stealing the oldest cell when full) — so a dropped packet is never read half-overwritten.
    Cell m_cells[kCap];
    std::atomic<size_t> m_enq{0}, m_deq{0};

    std::thread m_thread;
    std::atomic<bool> m_running{false};
    std::atomic<uint32_t> m_wake{0};
    std::atomic<uint64_t> m_dropped{0}, m_sent{0}, m_lookups{0};
    std::atomic<int> m_errKind{0}, m_errCode{0};   // see lastError()
};

} // namespace mikmap
