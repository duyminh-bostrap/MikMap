#include "TestHarness.h"
#include "io/sources/CameraSource.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>
using namespace mikmap;
namespace {
using Clock=std::chrono::steady_clock;
template<class F> bool until(F f,int ms=1000) { auto end=Clock::now()+std::chrono::milliseconds(ms); do { if(f()) return true; std::this_thread::sleep_for(std::chrono::milliseconds(2)); } while(Clock::now()<end); return false; }
std::unique_ptr<FakeCamera> camera(int w=16,int h=8) {
    auto c=std::make_unique<FakeCamera>(); FakeCamera::Frame f; f.width=w; f.height=h; f.rgb.assign(size_t(w)*h*3,73); f.tCaptureNs=123456789;
    c->setFrames({f}); c->setLoop(true); c->setFrameIntervalMs(33); return c;
}
class TimedCamera final : public ICameraDevice {
public:
    std::unique_ptr<FakeCamera> fake=camera();
    std::atomic<int64_t> newest{0};
    bool open(const std::string& id,int w,int h,std::string& e) override {return fake->open(id,w,h,e);}
    void close() override {fake->close();}
    CameraCaps caps() const override {return fake->caps();}
    bool read(CameraFrame& f,int timeout,std::string& e) override {
        if(!fake->read(f,timeout,e)) return false;
        f.tCaptureNs=std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now().time_since_epoch()).count(); newest=f.tCaptureNs; return true;
    }
};
int pose(const CameraFrame&,PersonPose* p,int,std::string&) {
    p[0]={}; p[0].score=1; for(int j=0;j<17;++j) p[0].kp[j]={.25f+float(j%2)*.1f,.4f+float(j)*.01f,.8f}; return 1;
}
}
TEST_CASE("CameraSource Off previews RGB resized and reuses output without ONNX") {
    CameraSourceConfig cfg; cfg.runtimeDir="nonexistent"; cfg.modelPath="nonexistent";
    CameraSource s(cfg,camera(1280,720)); REQUIRE(s.start()); CameraFrame out;
    REQUIRE(until([&]{return s.previewLatest(out);})); CHECK(out.width==640); CHECK(out.height==360); CHECK(out.rgb.size()==640*360*3); CHECK(out.rgb[0]==73); CHECK(out.tCaptureNs==123456789);
    auto ptr=out.rgb.data(); REQUIRE(until([&]{return s.previewLatest(out);})); CHECK(out.rgb.data()==ptr);
    REQUIRE(until([&]{return s.frames().consume();})); CHECK(s.frames().readSlot().count==0); CHECK(s.status()==SourceStatus::Running); CHECK(!s.stats().onCpu); CHECK(s.stats().camFps>0);
}
TEST_CASE("CameraSource Body needs runtime pack and load failure remains alive") {
    CameraSourceConfig cfg; cfg.mode=CamMode::Body; CameraSource a(cfg,camera()); CHECK(!a.start()); CHECK(a.status()==SourceStatus::Error); CHECK(a.lastError().find("Needs pack")!=std::string::npos);
    cfg.runtimeDir="missing-runtime"; cfg.modelPath="missing-model"; CameraSource b(cfg,camera()); CHECK(!b.start()); CHECK(b.status()==SourceStatus::Error); CHECK(!b.lastError().empty());
}
TEST_CASE("CameraSource Body preserves timestamps flip confidence and expires unplugged points") {
    CameraSourceConfig cfg; cfg.mode=CamMode::Body; cfg.flip=true; cfg.tracker.keepSec=.1;
    auto c=camera(); auto* dev=c.get(); dev->setUnplugAfter(5);
    CameraSource s(cfg,std::move(c),pose); REQUIRE(s.start());
    REQUIRE(until([&]{return s.frames().consume() && s.frames().readSlot().count>0;})); auto f=s.frames().readSlot(); CHECK(f.tCaptureNs==123456789); CHECK(f.count==17); CHECK_NEAR(f.points[0].x,.75,.0001); CHECK_NEAR(f.points[0].z,.8,.0001);
    REQUIRE(until([&]{return s.status()==SourceStatus::Error;})); CHECK(s.lastError().find("Camera")!=std::string::npos);
    REQUIRE(until([&]{return s.frames().consume() && s.frames().readSlot().count==0;},600)); CHECK(s.stats().people==0);
    dev->setUnplugAfter(-1); dev->replug(); REQUIRE(until([&]{return s.frames().consume() && s.frames().readSlot().count>0;},2500)); CHECK(s.status()==SourceStatus::Running);
}
TEST_CASE("CameraSource slow inference skips captures with bounded latency") {
    CameraSourceConfig cfg; cfg.mode=CamMode::Body;
    auto c=std::make_unique<TimedCamera>(); auto* dev=c.get(); std::atomic<uint64_t> index{0}; std::array<const uint8_t*,3> buffers{}; std::atomic<bool> bounded{true};
    CameraSource s(cfg,std::move(c),[&](const CameraFrame& f,PersonPose* p,int n,std::string& e){ index=f.index; bool found=false; for(auto& b:buffers) { if(!b) b=f.rgb.data(); if(b==f.rgb.data()) { found=true; break; } } if(!found) bounded=false; std::this_thread::sleep_for(std::chrono::milliseconds(100)); return pose(f,p,n,e); }); REQUIRE(s.start());
    REQUIRE(until([&]{return s.frames().publishedCount()>=6;},1500));
    CHECK(dev->fake->framesDelivered()>int(s.frames().publishedCount())*2); CHECK(uint64_t(dev->fake->framesDelivered())-index.load()<=5); REQUIRE(s.frames().consume()); CHECK(dev->newest-s.frames().readSlot().tCaptureNs<=150000000); CHECK(s.stats().latencyMs>=90); CHECK(s.stats().latencyMs<160); CHECK(s.stats().inferMs>=90); CHECK(s.stats().inferFps>0); CHECK(s.stats().people==1);
    auto start=Clock::now(); s.stop(); CHECK(Clock::now()-start<std::chrono::seconds(1)); CHECK(bounded);
}
TEST_CASE("CameraSource stop interrupts bounded camera wait and can restart") {
    CameraSource s({},std::make_unique<FakeCamera>()); REQUIRE(s.start()); std::this_thread::sleep_for(std::chrono::milliseconds(20)); auto start=Clock::now(); s.stop(); CHECK(Clock::now()-start<std::chrono::seconds(1)); CHECK(s.status()==SourceStatus::Stopped); REQUIRE(s.start()); s.stop();
}
TEST_CASE("CameraSource replay loops with ids confidence and validated records") {
    CameraSourceConfig cfg; cfg.replayPath=(std::filesystem::path(__FILE__).parent_path()/"data/replay_two_people.mikpose").string(); cfg.mode=CamMode::Body;
    CameraSource s(cfg); REQUIRE(s.start()); REQUIRE(until([&]{return s.frames().consume();})); auto f=s.frames().readSlot(); REQUIRE(f.count==2); CHECK(f.points[0].id==16); CHECK(f.points[1].id==116); CHECK_NEAR(f.points[0].z,.8,.0001); CHECK(s.stats().people==2); REQUIRE(until([&]{return s.frames().publishedCount()>=4;})); s.stop();
    auto path=std::filesystem::temp_directory_path()/"mikmap_bad_replay_task7.mikpose";
    for(const auto* line:{"0 30 16 .2 .3 .8","0 0 33 .2 .3 .8","0 0 16 .2 .3 2","nan 0 16 .2 .3 .8","0 0 16 inf .3 .8","0 0 16 .2 .3 .8 junk"}) {
        {std::ofstream file(path); file<<line<<'\n';} cfg.replayPath=path.string(); CameraSource bad(cfg); CHECK(!bad.start()); CHECK(bad.status()==SourceStatus::Error);
    }
    std::filesystem::remove(path);
}
TEST_CASE("CameraSource restart disconnected clears previously published points") {
    CameraSourceConfig cfg; cfg.mode=CamMode::Body;
    auto c=camera(); auto* dev=c.get(); CameraSource s(cfg,std::move(c),pose);
    REQUIRE(s.start()); REQUIRE(until([&]{return s.frames().consume() && s.frames().readSlot().count>0;})); s.stop();
    dev->setOpenError("disconnected"); REQUIRE(s.start());
    REQUIRE(until([&]{return s.frames().consume() && s.frames().readSlot().count==0;},600)); CHECK(s.stats().people==0);
}
TEST_CASE("CameraSource preview is the processed capture under slow inference") {
    CameraSourceConfig cfg; cfg.mode=CamMode::Body;
    CameraSource s(cfg,std::make_unique<TimedCamera>(),[](const CameraFrame& f,PersonPose* p,int n,std::string& e){std::this_thread::sleep_for(std::chrono::milliseconds(100)); return pose(f,p,n,e);});
    REQUIRE(s.start()); REQUIRE(until([&]{return s.frames().consume() && s.frames().readSlot().count>0;}));
    CameraFrame out; REQUIRE(s.previewLatest(out)); CHECK(out.tCaptureNs==s.frames().readSlot().tCaptureNs);
}
TEST_CASE("CameraSource replay bounds line input and stored frame bytes") {
    auto path=std::filesystem::temp_directory_path()/"mikmap_large_replay_task7.mikpose";
    CameraSourceConfig cfg; cfg.replayPath=path.string();
    {std::ofstream file(path); file<<std::string(1024,' ')<<"0 0 16 .2 .3 .8\n";}
    {CameraSource s(cfg); CHECK(!s.start()); CHECK(s.lastError().find("limit")!=std::string::npos);}
    {std::ofstream file(path); for(int i=0;i<6000;++i) file<<i<<" 0 16 .2 .3 .8\n\n";}
    {CameraSource s(cfg); CHECK(!s.start()); CHECK(s.lastError().find("limit")!=std::string::npos);}
    {std::ofstream file(path,std::ios::binary); file.seekp(16*1024*1024); file.put('\n');}
    {CameraSource s(cfg); CHECK(!s.start()); CHECK(s.lastError().find("limit")!=std::string::npos);}
    std::filesystem::remove(path);
}

TEST_CASE("CameraSource replay Off clears points and previews matching gray capture") {
    CameraSourceConfig cfg; cfg.replayPath=(std::filesystem::path(__FILE__).parent_path()/"data/replay_two_people.mikpose").string();
    CameraSource source(cfg); REQUIRE(source.start());
    REQUIRE(until([&]{return source.frames().consume();}));
    CHECK(source.frames().readSlot().count==0); CHECK(source.stats().people==0);
    CameraFrame preview; preview.rgb.reserve(640*480*3);
    REQUIRE(until([&]{return source.previewLatest(preview);}));
    CHECK(preview.width==640); CHECK(preview.height==480); CHECK(preview.rgb.size()==640*480*3);
    CHECK(preview.tCaptureNs==source.frames().readSlot().tCaptureNs);
    auto* buffer=preview.rgb.data(); REQUIRE(until([&]{return source.previewLatest(preview);})); CHECK(buffer==preview.rgb.data());
    CHECK(source.stats().camFps>0); CHECK(source.stats().inferMs==0);
}
