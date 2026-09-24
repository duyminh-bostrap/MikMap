// ════════════════════════════════════════════════════════════════════════
//  io/SensorLog.h — ghi lại và phát lại phiên sensor (G16)
//
//  ── Vấn đề nó giải ───────────────────────────────────────────────────
//  Lỗi sensor gần như không bao giờ tái hiện được ở bàn làm việc. Nó xảy
//  ra lúc 11 giờ đêm, giữa buổi diễn, với đúng cái LiDAR đó, đúng cách
//  người ta bước qua vùng quét đó. Hôm sau ngồi mở máy ra thì mọi thứ
//  chạy hoàn hảo.
//
//  Ghi lại các gói THÔ nghĩa là mang được nguyên hiện trường về: phát lại
//  đúng chuỗi gói, đúng khoảng cách thời gian, không cần phần cứng, và
//  chạy được trong unit test.
//
//  ── ★ Vì sao ghi GÓI THÔ chứ không ghi SensorFrame đã xử lý ──────────
//  Ghi SensorFrame thì chỉ phát lại được phần SAU bộ giải mã — mà phần
//  lớn lỗi nằm ở chính chỗ đó: gói dị dạng, thứ tự đảo, id trùng, alive
//  thiếu. Ghi byte thô giữ được đúng thứ người thật đã gửi, kể cả những
//  gói mà bản hiện tại của phần mềm còn đang hiểu sai.
//
//  Đổi lại file to hơn — nhưng một phiên 2 giờ ở 40 Hz với gói ~200 byte
//  là khoảng 60 MB. Không đáng để đánh đổi lấy khả năng truy lỗi.
//
//  ── Định dạng ────────────────────────────────────────────────────────
//    [8]  magic "HEXSLOG1"
//    [4]  số nguyên: phiên bản định dạng (little-endian)
//    lặp:
//      [8]  int64  mốc thời gian, NANO GIÂY tính từ gói đầu tiên
//      [4]  uint32 độ dài gói
//      [n]  byte   nội dung gói, nguyên xi
//
//  Mốc thời gian là ĐỘ LỆCH chứ không phải giờ tuyệt đối: file phát lại
//  được ở bất kỳ thời điểm nào, và không vô tình mang theo thông tin về
//  lúc nào ở đâu.
// ════════════════════════════════════════════════════════════════════════
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace mikmap {

/// Một gói đã ghi.
struct LoggedPacket {
    int64_t              tOffsetNs = 0;   ///< lệch so với gói đầu tiên
    std::vector<uint8_t> data;
};

/// Ghi phiên ra file. An toàn khi mở file thất bại — khi đó mọi lời gọi
/// `write` chỉ đơn giản không làm gì.
///
/// ★ Không dùng trong thread sensor mà không suy nghĩ: đây là I/O chặn.
///   Hiện chỉ dùng ở luồng công cụ / test; nếu sau này ghi lúc đang diễn
///   thì phải đẩy qua hàng đợi sang thread riêng.
class SensorLogWriter {
public:
    ~SensorLogWriter();

    bool open(const std::string& path);
    bool isOpen() const { return m_file != nullptr; }
    void close();

    /// @param tNs mốc thời gian đơn điệu (steady_clock). Gói ĐẦU TIÊN đặt
    ///        gốc; các gói sau ghi độ lệch so với nó.
    void write(int64_t tNs, const uint8_t* data, size_t size);

    uint64_t packetCount() const { return m_count; }

private:
    void*    m_file = nullptr;    ///< FILE*, giấu đi để header không kéo <cstdio>
    int64_t  m_tFirst = 0;
    bool     m_haveFirst = false;
    uint64_t m_count = 0;
};

/// Đọc toàn bộ file log.
///
/// @param outWarning  ghi rõ khi file cụt hoặc hỏng giữa chừng
/// @return false chỉ khi KHÔNG mở được hoặc không phải file log.
///         File cụt giữa chừng vẫn trả về true kèm cảnh báo — một phiên
///         ghi dở vì mất điện vẫn là bằng chứng dùng được, và đó chính là
///         loại phiên hay cần xem lại nhất.
bool readSensorLog(const std::string& path,
                   std::vector<LoggedPacket>& out,
                   std::string& outWarning);

/// Phát lại: chọn các gói đã tới hạn tính từ lúc bắt đầu.
///
/// Tách khỏi thread và đồng hồ để test được: người gọi đưa vào "đã trôi
/// bao lâu", nhận về các gói cần bơm.
///
/// @param elapsedNs  thời gian đã trôi kể từ lúc bắt đầu phát
/// @param cursor     vị trí đọc, người gọi giữ giữa các lần gọi
/// @return số gói vừa được thêm vào `out`
size_t collectDuePackets(const std::vector<LoggedPacket>& log,
                         int64_t elapsedNs,
                         size_t& cursor,
                         std::vector<const LoggedPacket*>& out);

} // namespace mikmap
