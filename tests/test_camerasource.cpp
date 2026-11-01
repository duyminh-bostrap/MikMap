#include "TestHarness.h"
#include "io/sources/CameraSource.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>
#include <limits>
#include <cstdlib>
#include <iostream>
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
TEST_CASE("CameraSource Face retains the four strongest objects") {
    CameraSourceConfig cfg; cfg.mode=CamMode::Face;
    CameraSource source(cfg,camera(),[](const CameraFrame&,PoseObject* out,int cap,std::string&) {
        const int n=std::min(8,cap);
        for(int i=0;i<n;++i) { out[i]={}; out[i].nkp=106; out[i].score=float(i+1)/8;
            for(auto& k:out[i].kp) k={float(i+1)/10,.4f,.8f}; }
        return n;
    });
    REQUIRE(source.start()); REQUIRE(until([&]{return source.frames().consume();}));
    const auto f=source.frames().readSlot(); REQUIRE(f.count==424);
    for(int i=0;i<f.count;++i) { CHECK(f.points[i].group==kGroupFace); CHECK(f.points[i].x>=.5f); }
    CHECK(source.stats().people==4); CHECK(source.stats().objectsCap==4); CHECK(source.stats().calPoint==kCalibUnavailable);
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

TEST_CASE("CameraSource Hand geometry flip and stable slots share tracker") {
    for(bool flip:{false,true}) {
        CameraSourceConfig cfg; cfg.mode=CamMode::Hand; cfg.flip=flip;
        auto dev=camera(); std::atomic<int> calls{0};
        CameraSource source(cfg,std::move(dev),[&](const CameraFrame&,PoseObject* out,int,std::string&) {
            const int call=++calls;
            for(int i=0;i<2;++i) {
                auto& p=out[i]; p={}; p.nkp=21; p.score=1.f-float(i)*.1f;
                const float x=.2f+float(i)*.5f;
                for(int k=0;k<21;++k) p.kp[k]={x+float(k%3)*.03f,.3f+float(k%5)*.02f,.8f};
                p.kp[0]={x,.3f,.8f}; p.kp[5]={x+.1f,.3f,.8f};
                // One opposite vote must not flip the established side.
                const bool right=(i==0)!=(call==4);
                p.kp[17]={x+.05f,right?.4f:.2f,.8f};
            }
            return 2;
        });
        REQUIRE(source.start());
        REQUIRE(until([&]{return calls>=3 && source.frames().consume();}));
        auto first=source.frames().readSlot(); REQUIRE(first.count==42);
        CHECK(first.points[0].group==(flip?kGroupHandL:kGroupHandR));
        CHECK(first.points[0].id==(flip?0u:50u));
        CHECK(first.points[21].group==(flip?kGroupHandR:kGroupHandL));
        CHECK(first.points[21].id==(flip?150u:100u));
        CHECK_NEAR(first.points[0].x,flip?.8:.2,.0001); CHECK_NEAR(first.points[0].z,.8,.0001);
        REQUIRE(until([&]{return calls>=5 && source.frames().consume();}));
        CHECK(source.frames().readSlot().points[0].id==first.points[0].id);
        source.stop(); calls=0; REQUIRE(source.start());
        REQUIRE(until([&]{return source.frames().consume() && source.frames().readSlot().count==42;}));
        CHECK(source.frames().readSlot().points[0].id==first.points[0].id);
    }
}
TEST_CASE("CameraSource Full caps before group filtering and keeps strongest objects") {
    for(uint8_t groups:{uint8_t(31),uint8_t(23),uint8_t(0)}) {
        CameraSourceConfig cfg; cfg.mode=CamMode::Full; cfg.maxObjects=30; cfg.fullGroups=groups;
        CameraSource source(cfg,camera(),[](const CameraFrame&,PoseObject* out,int,std::string&) {
            for(int i=0;i<6;++i) { out[i]={}; out[i].nkp=133; out[i].score=float(i+1)/6;
                for(auto& k:out[i].kp) k={float(i+1)/10,.4f,.7f}; }
            return 6;
        });
        REQUIRE(source.start()); REQUIRE(until([&]{return source.frames().consume();}));
        auto f=source.frames().readSlot(); CHECK(f.count==(groups==31?399:groups==23?195:0));
        CHECK(source.stats().people==3); CHECK(source.stats().objectsCap==3);
        int counts[5]{};
        for(int i=0;i<f.count;++i) { REQUIRE(f.points[i].group<5); ++counts[f.points[i].group]; CHECK(f.points[i].x>=.4f); CHECK_NEAR(f.points[i].z,.7,.0001); }
        if(groups) { CHECK(counts[0]==51); CHECK(counts[1]==63); CHECK(counts[2]==63); CHECK(counts[4]==18); CHECK(counts[3]==(groups==31?204:0)); }
    }
}
TEST_CASE("CameraSource validates injected layout count and finite points") {
    for(int bad:{0,1,2}) {
        CameraSourceConfig cfg; cfg.mode=CamMode::Face;
        CameraSource source(cfg,camera(),[bad](const CameraFrame&,PoseObject* out,int cap,std::string&) {
            out[0]={}; out[0].score=1; out[0].nkp=bad==0?17:106;
            for(auto& k:out[0].kp) k={.2f,.4f,.8f};
            return bad==1?cap+1:bad==2?-1:1;
        });
        REQUIRE(source.start()); REQUIRE(until([&]{return source.frames().consume();}));
        CHECK(source.frames().readSlot().count==0); CHECK(source.status()==SourceStatus::Error);
    }
    CameraSourceConfig cfg; cfg.mode=CamMode::Body;
    CameraSource source(cfg,camera(),[](const CameraFrame& f,PoseObject* out,int cap,std::string& err) {
        pose(f,out,cap,err); out[0].kp[0]={-2,2,2};
        out[0].kp[1].conf=std::numeric_limits<float>::quiet_NaN(); out[0].kp[2].x=std::numeric_limits<float>::infinity(); return 1;
    });
    REQUIRE(source.start()); REQUIRE(until([&]{return source.frames().consume();})); auto f=source.frames().readSlot();
    REQUIRE(f.count==15); CHECK(f.points[0].x==0); CHECK(f.points[0].y==1); CHECK(f.points[0].z==1);
}
TEST_CASE("CameraSource zero capacity skips inference and Color requirement is enforced") {
    for(auto mode:{CamMode::Body,CamMode::Hand,CamMode::Face,CamMode::Full}) {
        CameraSourceConfig cfg; cfg.mode=mode; cfg.maxObjects=0; std::atomic<int> calls{0};
        CameraSource source(cfg,camera(),[&](const CameraFrame&,PoseObject*,int,std::string&){++calls; return 0;});
        REQUIRE(source.start()); REQUIRE(until([&]{return source.frames().consume();}));
        CHECK(source.frames().readSlot().count==0); CHECK(source.stats().objectsCap==0); CHECK(source.stats().people==0); CHECK(calls==0);
        CameraSource noColor(cfg,std::make_unique<FakeCamera>(CameraCaps{}),pose);
        REQUIRE(noColor.start()); REQUIRE(until([&]{return noColor.status()==SourceStatus::Error;}));
        CHECK(noColor.lastError().find("Color")!=std::string::npos);
    }
}
TEST_CASE("CameraSource grouped replay maps local ids filters and counts face slots safely") {
    CameraSourceConfig cfg; cfg.replayPath=(std::filesystem::path(__FILE__).parent_path()/"data/replay_modes.mikpose").string();
    cfg.mode=CamMode::Full;
    { CameraSource source(cfg); REQUIRE(source.start()); REQUIRE(until([&]{return source.frames().consume();}));
      auto f=source.frames().readSlot(); REQUIRE(f.count==5); CHECK(f.points[1].id==0); CHECK(f.points[2].id==50);
      CHECK(f.points[3].id==2105); CHECK(f.points[3].group==kGroupFace); CHECK(source.stats().people==2); }
    cfg.fullGroups=23; cfg.maxObjects=1;
    { CameraSource source(cfg); REQUIRE(source.start()); REQUIRE(until([&]{return source.frames().consume();}));
      auto f=source.frames().readSlot(); REQUIRE(f.count==4); for(int i=0;i<f.count;++i) CHECK(f.points[i].group!=kGroupFace); CHECK(source.stats().people==1); }
    cfg.mode=CamMode::Face; cfg.maxObjects=10;
    { CameraSource source(cfg); REQUIRE(source.start()); REQUIRE(until([&]{return source.frames().consume();}));
      auto f=source.frames().readSlot(); REQUIRE(f.count==1); CHECK(f.points[0].id==2105); CHECK(source.stats().people==1); }
    cfg.mode=CamMode::Hand;
    { CameraSource source(cfg); REQUIRE(source.start()); REQUIRE(until([&]{return source.frames().consume();})); CHECK(source.frames().readSlot().count==2); }
}
TEST_CASE("CameraSource grouped replay rejects invalid group ranges duplicates and trailing tokens") {
    auto path=std::filesystem::temp_directory_path()/"mikmap_bad_replay_modes.mikpose";
    CameraSourceConfig cfg; cfg.replayPath=path.string(); cfg.mode=CamMode::Full;
    for(const auto* lines:{"0 0 0 .2 .3 .8 -1", "0 0 0 .2 .3 .8 5", "0 0 0 .2 .3 .8 1 junk",
         "0 0 0 .2 .3 .8 1.5", "0 0 21 .2 .3 .8 1", "0 0 50 .2 .3 .8 2", "0 0 106 .2 .3 .8 3",
         "0 4 0 .2 .3 .8 3", "0 24 0 .2 .3 .8 2", "0 0 6 .2 .3 .8 4", "0 3 0 .2 .3 .8 4",
         "0 0 0 .2 .3 .8 1\n0 0 0 .2 .3 .8 1"}) {
        {std::ofstream file(path); file<<lines<<'\n';} CameraSource source(cfg); CHECK(!source.start());
    }
    std::filesystem::remove(path);
}

TEST_CASE("CameraSource optional real CPU top-down sessions and Face metadata") {
    const auto env=[](const char* name) {
#ifdef _MSC_VER
        char* raw=nullptr; size_t len=0; if(_dupenv_s(&raw,&len,name)!=0) return std::string{};
        std::string value=raw?raw:""; std::free(raw); return value;
#else
        const auto* raw=std::getenv(name); return std::string(raw?raw:"");
#endif
    };
    const auto dir=env("MIKMAP_TEST_ORT_DIR"),fixtures=env("MIKMAP_TEST_TOPDOWN_FIXTURES");
    if(dir.empty() || fixtures.empty()) { std::cout<<"[SKIP] CameraSource real CPU fixtures\n"; return; }
    CameraSourceConfig cfg; cfg.runtimeDir=dir; cfg.device=OrtDevice::Cpu;
    // Coincident fixture landmarks cannot match by IoU; cap tracks to the two detector objects.
    cfg.maxObjects=2;
    cfg.detectorPath=(std::filesystem::path(fixtures)/"detector.onnx").string();
    for(auto mode:{CamMode::Hand,CamMode::Face,CamMode::Full}) {
        cfg.mode=mode; cfg.keypointPath=(std::filesystem::path(fixtures)/("keypoints-"+std::to_string(modeSpec(mode).keypointsPerObject)+".onnx")).string();
        CameraSource source(cfg,camera(640,640)); REQUIRE(source.start());
        REQUIRE(until([&]{return source.frames().publishedCount()>=2 && source.frames().consume();},5000));
        CHECK(source.status()==SourceStatus::Running); CHECK(source.stats().people==2); CHECK(source.stats().onCpu);
        CHECK(source.lastError().find("CPU")!=std::string::npos);
        // Constant coincident hand landmarks have unknown side and are intentionally not emitted.
        CHECK(source.frames().readSlot().count==(mode==CamMode::Hand?0:mode==CamMode::Face?212:266));
        if(mode==CamMode::Face) CHECK(source.stats().calPoint==kCalibUnavailable);
    }
    cfg.mode=CamMode::Face;
    for(const auto* suffix:{"metadata","trailing","range","negative","space"}) {
        cfg.keypointPath=(std::filesystem::path(fixtures)/(std::string("keypoints-106-")+suffix+".onnx")).string();
        CameraSource source(cfg,camera(640,640)); REQUIRE(source.start());
        CHECK(source.stats().calPoint==(std::string(suffix)=="metadata"?7u:kCalibUnavailable));
        REQUIRE(until([&]{return source.frames().publishedCount()>=2 && source.frames().consume();},5000)); CHECK(source.frames().readSlot().count==212);
    }
    cfg.device=OrtDevice::Auto;
    {CameraSource source(cfg,camera(640,640)); REQUIRE(source.start()); CHECK(source.stats().onCpu);}
    cfg.device=OrtDevice::CudaOnly;
    {CameraSource source(cfg,camera()); CHECK(!source.start()); CHECK(source.status()==SourceStatus::Error); CHECK(!source.lastError().empty());}
    cfg.device=OrtDevice::Cpu; cfg.keypointPath=(std::filesystem::path(fixtures)/"keypoints-21.onnx").string();
    {CameraSource source(cfg,camera()); CHECK(!source.start()); CHECK(source.status()==SourceStatus::Error);}
}

TEST_CASE("CameraSource maximum mode output fits fixed frame and user cap overrides tracker cap") {
    for(auto mode:{CamMode::Body,CamMode::Hand,CamMode::Face,CamMode::Full}) {
        CameraSourceConfig cfg; cfg.mode=mode; cfg.maxObjects=30; cfg.tracker.maxPeople=1;
        CameraSource source(cfg,camera(),[mode](const CameraFrame&,PoseObject* out,int cap,std::string&) {
            for(int i=0;i<cap;++i) { auto& p=out[i]; p={}; p.nkp=modeSpec(mode).keypointsPerObject; p.score=1;
                for(auto& k:p.kp) k={.2f,.3f,.8f};
                if(mode==CamMode::Hand) { p.kp[5].x=.3f; p.kp[17].y=.4f; }
            }
            return cap;
        });
        REQUIRE(source.start()); REQUIRE(until([&]{return source.frames().consume();}));
        const auto& spec=modeSpec(mode); CHECK(source.stats().people==spec.objectsCap);
        CHECK(source.frames().readSlot().count==spec.objectsCap*spec.keypointsPerObject);
        CHECK(source.frames().readSlot().count<=kMaxTouchPoints);
    }
}
