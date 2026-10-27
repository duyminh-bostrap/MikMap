#include "TestHarness.h"

#include "io/plugin/PluginLoader.h"
#include "io/sources/PluginSource.h"

#include <chrono>
#include <cstdint>
#include <string>
#include <thread>

#ifndef FAKE_PLUGIN_PATH
#  error "FAKE_PLUGIN_PATH must be defined by CMake (mikmap_tests)"
#endif
#ifndef FAKE_PLUGIN_V999_PATH
#  error "FAKE_PLUGIN_V999_PATH must be defined by CMake (mikmap_tests)"
#endif

using namespace mikmap;

namespace {

template <typename Pred>
bool waitFor(Pred pred, int ms) {
    const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    while (std::chrono::steady_clock::now() < end) {
        if (pred()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return pred();
}

} // namespace

TEST_CASE("Plugin: nap duoc, dung ten va config mac dinh") {
    SensorPlugin p;
    std::string err;
    REQUIRE(loadSensorPlugin(FAKE_PLUGIN_PATH, p, err));
    CHECK(p.name == "Fake scan");
    CHECK(p.defaultConfig.find("fail_after") != std::string::npos);
    CHECK(err.empty());
    CHECK(p.api_version != nullptr);
    CHECK(p.poll != nullptr);
}

TEST_CASE("Plugin: lech phien ban API bi tu choi") {
    SensorPlugin p;
    std::string err;
    REQUIRE(!loadSensorPlugin(FAKE_PLUGIN_V999_PATH, p, err));
    CHECK(err.find("API version 999") != std::string::npos);
}

TEST_CASE("Plugin: duong dan khong ton tai") {
    const std::string path = "definitely_missing_mikmap_plugin.dll";
    SensorPlugin p;
    std::string err;
    REQUIRE(!loadSensorPlugin(path, p, err));
    CHECK(err.find(path) != std::string::npos);
}

TEST_CASE("★ Plugin: scan → 1 touch qua cung pipeline voi Hokuyo") {
    SensorPlugin p;
    std::string err;
    REQUIRE(loadSensorPlugin(FAKE_PLUGIN_PATH, p, err));

    PluginSource src(p, {"{\"fail_after\":0}", 0.1});
    REQUIRE(src.start());

    REQUIRE(waitFor([&] {
        return src.status() == SourceStatus::Running && src.scansReceived() >= 3;
    }, 2000));

    REQUIRE(src.scans().consume());
    REQUIRE(src.scans().readSlot().count == 1081);

    bool one = false;
    REQUIRE(waitFor([&] {
        if (src.frames().consume()) one = (src.frames().readSlot().count == 1);
        return one;
    }, 1000));
    REQUIRE(one);

    const float x = src.frames().readSlot().points[0].x;
    CHECK(x > 975.0f);
    CHECK(x < 1025.0f);

    src.stop();
}

TEST_CASE("★ Plugin: poll loi → Up, Error co thong bao, tu tao lai") {
    SensorPlugin p;
    std::string err;
    REQUIRE(loadSensorPlugin(FAKE_PLUGIN_PATH, p, err));

    PluginSource src(p, {"{\"fail_after\":5}", 0.1});
    REQUIRE(src.start());

    uint32_t downId = 0;
    bool upSeen = false, errSeen = false;

    const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (std::chrono::steady_clock::now() < end) {
        TouchEvent e;
        while (src.events().pop(e)) {
            if (e.state == TouchState::Down) downId = e.id;
            if (e.state == TouchState::Up && downId != 0 && e.id == downId) upSeen = true;
        }
        if (src.lastError().find("cable pulled") != std::string::npos) errSeen = true;
        if (upSeen && errSeen && src.scansReceived() > 5) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    CHECK(downId != 0);
    CHECK(upSeen);
    CHECK(errSeen);
    CHECK(src.scansReceived() > 5);

    src.stop();
}
