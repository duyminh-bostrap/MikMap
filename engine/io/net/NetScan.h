// ════════════════════════════════════════════════════════════════════════
//  io/net/NetScan.h — tìm thiết bị TCP trong mạng LAN (wizard thêm sensor)
//
//  Chỉ STL + API mạng của OS. Mọi hàm đều CHẶN (scanTcpPort tới ~timeoutMs
//  mỗi lô 256 host): gọi từ thread riêng, không bao giờ từ render thread.
// ════════════════════════════════════════════════════════════════════════
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace mikmap {

struct Ipv4Iface { std::string name, address; int prefix = 24; };

/// Card mạng IPv4 đang up, bỏ loopback (GetAdaptersAddresses / getifaddrs).
std::vector<Ipv4Iface> localIpv4Interfaces();

/// Mọi host trong subnet của `address`/`prefix`, bỏ địa chỉ mạng, broadcast
/// và chính `address`. prefix < 22 (quá 1022 host) hoặc địa chỉ hỏng ⇒ rỗng.
std::vector<std::string> hostsInSubnet(const std::string& address, int prefix);

/// Thử connect TCP non-blocking tới `port` theo lô 256, chờ bằng select/poll.
/// Trả các host nhận kết nối, theo thứ tự đầu vào.
std::vector<std::string> scanTcpPort(const std::vector<std::string>& hosts, uint16_t port, int timeoutMs);

} // namespace mikmap
