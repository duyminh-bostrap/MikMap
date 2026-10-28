#include "TestHarness.h"

#include "io/net/NetScan.h"
#include "io/net/Socket.h"

#include <algorithm>
#include <string>
#include <vector>

using namespace mikmap;

namespace {
bool has(const std::vector<std::string>& v, const std::string& s) { return std::find(v.begin(), v.end(), s) != v.end(); }
}

TEST_CASE("NetScan: hostsInSubnet /24 bo mang, broadcast va chinh minh") {
    const auto h = hostsInSubnet("192.168.0.23", 24);
    CHECK(h.size() == 253u);
    CHECK(has(h, "192.168.0.1"));
    CHECK(has(h, "192.168.0.254"));
    CHECK(!has(h, "192.168.0.0"));
    CHECK(!has(h, "192.168.0.255"));
    CHECK(!has(h, "192.168.0.23"));
}

TEST_CASE("NetScan: hostsInSubnet /30 chi con mot host") {
    CHECK(hostsInSubnet("10.0.0.21", 30) == std::vector<std::string>{"10.0.0.22"});
}

TEST_CASE("NetScan: hostsInSubnet tu choi dai qua lon va dia chi hong") {
    CHECK(hostsInSubnet("10.0.0.1", 16).empty());
    CHECK(hostsInSubnet("not.an.ip", 24).empty());
}

TEST_CASE("NetScan: hostsInSubnet /31 /32 rong, khong tran o 255.255.255.255") {
    CHECK(hostsInSubnet("255.255.255.255", 32).empty());
    CHECK(hostsInSubnet("255.255.255.254", 31).empty());
    CHECK(hostsInSubnet("10.0.0.1", 32).empty());
    CHECK(hostsInSubnet("255.255.255.253", 30) == std::vector<std::string>{"255.255.255.254"});   // the last /30 of the address space
}

TEST_CASE("NetScan: scanTcpPort thay cong dang listen, bo cong dong") {
    uint16_t port = 0;
    std::string err;
    const SocketHandle l = tcpListenLoopback(0, port, err);
    REQUIRE(l != kInvalidSocket);
    CHECK(scanTcpPort({"127.0.0.1"}, port, 300) == std::vector<std::string>{"127.0.0.1"});
    closeSocketHandle(l);
    CHECK(scanTcpPort({"127.0.0.1"}, port, 300).empty());   // nobody listens there any more
}

TEST_CASE("NetScan: localIpv4Interfaces khong co loopback") {
    const auto ifs = localIpv4Interfaces();
    for (const auto& i : ifs) {
        CHECK(i.address != "127.0.0.1");
        CHECK(i.prefix >= 0);
        CHECK(i.prefix <= 32);
    }
}
