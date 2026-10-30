// ════════════════════════════════════════════════════════════════════════
//  tests/FakeHttpServer.h — máy chủ HTTP 127.0.0.1 nhẹ cho unit test
// ════════════════════════════════════════════════════════════════════════
#pragma once

#include "io/net/Socket.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <map>
#include <mutex>
#include <string>
#include <thread>

namespace mikmap::test {

class FakeHttpServer {
public:
    struct Response {
        int         status       = 200;
        std::string body;
        int         chunkSize    = 0;   // 0 = gửi một lần
        int         chunkDelayMs = 0;
        std::string redirectTo;
    };

    FakeHttpServer() {
        std::string err;
        m_listener = tcpListenLoopback(0, m_port, err);
        if (m_listener != kInvalidSocket) {
            m_thread = std::thread([this]() { serveLoop(); });
        }
    }

    ~FakeHttpServer() {
        m_stop.store(true);
        if (m_listener != kInvalidSocket) {
            closeSocketHandle(m_listener);
            m_listener = kInvalidSocket;
        }
        if (m_thread.joinable()) m_thread.join();
    }

    FakeHttpServer(const FakeHttpServer&) = delete;
    FakeHttpServer& operator=(const FakeHttpServer&) = delete;

    uint16_t port() const { return m_port; }
    std::string url(const std::string& path) const {
        return "http://127.0.0.1:" + std::to_string(m_port) + path;
    }

    void addRoute(const std::string& path, Response resp) {
        std::lock_guard<std::mutex> lk(m_mu);
        m_routes[path] = std::move(resp);
    }

    void addRoute(const std::string& path, std::string body, int status = 200) {
        Response r;
        r.status = status;
        r.body   = std::move(body);
        addRoute(path, std::move(r));
    }

    int hits(const std::string& path) const {
        std::lock_guard<std::mutex> lk(m_mu);
        auto it = m_hits.find(path);
        return it == m_hits.end() ? 0 : it->second;
    }

private:
    void serveLoop() {
        while (!m_stop.load()) {
            SocketHandle client = tcpAccept(m_listener, 30);
            if (client == kInvalidSocket) continue;
            handleClient(client);
            shutdownSocket(client);
            closeSocketHandle(client);
        }
    }

    void handleClient(SocketHandle client) {
        setRecvTimeoutMs(client, 2000);
        std::string req;
        char buf[1024];
        while (req.find("\r\n\r\n") == std::string::npos && req.size() < 8192) {
            int n = 0;
            if (recvSome(client, buf, sizeof(buf), n) != RecvResult::Data) break;
            req.append(buf, static_cast<size_t>(n));
        }
        // Tách dòng đầu: "GET /path HTTP/1.1"
        std::string path = "/";
        const size_t sp1 = req.find(' ');
        if (sp1 != std::string::npos) {
            const size_t sp2 = req.find(' ', sp1 + 1);
            if (sp2 != std::string::npos) path = req.substr(sp1 + 1, sp2 - sp1 - 1);
        }

        Response resp;
        resp.status = 404;
        resp.body   = "not found";
        {
            std::lock_guard<std::mutex> lk(m_mu);
            m_hits[path]++;
            auto it = m_routes.find(path);
            if (it != m_routes.end()) resp = it->second;
        }

        const char* reason = (resp.status == 200) ? "OK"
                           : (resp.status == 302) ? "Found"
                           : (resp.status == 404) ? "Not Found"
                                                  : "Error";
        std::string hdr = "HTTP/1.1 " + std::to_string(resp.status) + " " + reason + "\r\n";
        if (!resp.redirectTo.empty()) hdr += "Location: " + resp.redirectTo + "\r\n";
        hdr += "Content-Length: " + std::to_string(resp.body.size()) + "\r\n";
        hdr += "Connection: close\r\n\r\n";
        if (!sendAll(client, hdr.data(), hdr.size())) return;

        if (resp.chunkSize <= 0) {
            sendAll(client, resp.body.data(), resp.body.size());
        } else {
            size_t pos = 0;
            while (pos < resp.body.size() && !m_stop.load()) {
                const size_t take = std::min(static_cast<size_t>(resp.chunkSize), resp.body.size() - pos);
                if (!sendAll(client, resp.body.data() + pos, take)) break;
                pos += take;
                if (pos < resp.body.size() && resp.chunkDelayMs > 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(resp.chunkDelayMs));
                }
            }
        }
    }

    SocketHandle                           m_listener = kInvalidSocket;
    uint16_t                               m_port     = 0;
    std::atomic<bool>                      m_stop{false};
    std::thread                            m_thread;
    mutable std::mutex                     m_mu;
    std::map<std::string, Response>        m_routes;
    mutable std::map<std::string, int>     m_hits;
};

} // namespace mikmap::test
