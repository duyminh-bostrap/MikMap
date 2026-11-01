#include "CameraSource.h"
#include "io/camera/MfCamera.h"
#include "io/ai/TopDownPose.h"
#include <charconv>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <thread>
namespace mikmap {
namespace {
using Clock=std::chrono::steady_clock;
int64_t nowNs() { return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now().time_since_epoch()).count(); }
struct Diagnostic { std::array<char,512> text{}; };
void message(TripleBuffer<Diagnostic>& channel,const std::string& text) {
    std::snprintf(channel.writeSlot().text.data(),512,"%s",text.c_str()); channel.publish();
}
bool valid(const CameraFrame& f) { return f.width>0 && f.height>0 && f.width<=16384 && f.height<=16384 && f.rgb.size()==size_t(f.width)*size_t(f.height)*3; }
void preview(const CameraFrame& f,CameraFrame& p,bool flip) {
    p.width=std::min(640,f.width); p.height=std::max(1,int(int64_t(f.height)*p.width/f.width));
    p.rgb.resize(size_t(p.width)*size_t(p.height)*3); p.depthMm.clear(); p.depthW=p.depthH=0; p.index=f.index; p.tCaptureNs=f.tCaptureNs;
    for(int y=0;y<p.height;++y) for(int x=0;x<p.width;++x) {
        int sx=x*f.width/p.width; if(flip) sx=f.width-1-sx;
        const size_t src=(size_t(y*f.height/p.height)*size_t(f.width)+size_t(sx))*3;
        const size_t dst=(size_t(y)*size_t(p.width)+size_t(x))*3;
        std::copy_n(f.rgb.data()+src,3,p.rgb.data()+dst);
    }
}
}
struct CameraSource::Impl {
    CameraSource& owner;
    CameraSourceConfig cfg;
    std::unique_ptr<ICameraDevice> device;
    Inference inference;
    OrtRuntime runtime, keypointRuntime;
    TopDownPose topDown;
    PoseModel model;
    PersonTracker tracker;
    TripleBuffer<CameraFrame> captured, previews;
    mutable TripleBuffer<Diagnostic> captureError, inferError;
    std::atomic<bool> running{false},cameraOk{false},captureFailed{false},inferOk{true},onCpu{false};
    std::atomic<float> camFps{0},inferFps{0},inferMs{0},latencyMs{0};
    std::atomic<int> people{0},objectsCap{0};
    std::atomic<uint32_t> calPoint{kCalibUnavailable};
    std::thread captureThread,inferThread;
    std::string startError;
    struct Replay { double time=0; SensorFrame frame; };
    std::vector<Replay> replay;
    uint64_t seq=0;
    Impl(CameraSource& o,CameraSourceConfig c,std::unique_ptr<ICameraDevice> d,Inference i):owner(o),cfg(std::move(c)),device(std::move(d)),inference(std::move(i)) {}
    bool wait(double seconds) {
        const auto end=Clock::now()+std::chrono::duration<double>(seconds);
        while(running && Clock::now()<end) std::this_thread::sleep_for(std::chrono::milliseconds(2));
        return running;
    }
    bool loadReplay() {
        replay.clear(); std::ifstream file(std::filesystem::path(std::u8string(cfg.replayPath.begin(),cfg.replayPath.end())),std::ios::binary|std::ios::ate);
        if(!file) { startError="Replay: cannot open file"; return false; }
        // Fixed limits bound both input parsing and allocated replay frame capacity.
        constexpr size_t maxInputBytes=16*1024*1024, maxFrames=64*1024*1024/sizeof(Replay);
        const auto fileBytes=file.tellg();
        if(fileBytes<0 || fileBytes>std::streamoff(maxInputBytes)) { startError="Replay: input limit is 16 MiB"; return false; }
        file.seekg(0);
        Replay current; bool active=false; double previous=-1; size_t inputBytes=0;
        std::array<char,512> record{};
        auto finish=[&] {
            if(active) {
                if(replay.size()>=maxFrames) { startError="Replay: stored frame limit is 64 MiB"; return false; }
                if(replay.size()==replay.capacity()) replay.reserve(std::min(maxFrames,std::max(size_t(1),replay.capacity()*2)));
                replay.push_back(current); previous=current.time; current={}; active=false;
            }
            return true;
        };
        while(file.getline(record.data(),std::streamsize(record.size())) || file.gcount()>0) {
            inputBytes+=size_t(file.gcount());
            if(file.fail() && !file.eof()) { startError="Replay: record limit is 511 bytes"; return false; }
            if(inputBytes>maxInputBytes) { startError="Replay: input limit is 16 MiB"; return false; }
            std::string line(record.data(),size_t(file.gcount())-(file.eof()?0:1));
            if(line.find_first_not_of(" \t\r")==std::string::npos) { if(!finish()) return false; continue; }
            std::istringstream in(line); double t; int slot,joint,group=0; TouchPoint p;
            bool parsed=bool(in>>t>>slot>>joint>>p.x>>p.y>>p.z);
            if(parsed) { in>>std::ws; if(!in.eof()) { parsed=bool(in>>group); if(parsed) { in>>std::ws; parsed=in.eof(); } } }
            const int jointCaps[]={33,21,21,106,6},slotCaps[]={30,24,24,4,3};
            if(!parsed || group<0 || group>4 || !std::isfinite(t) || t<0 || t>86400 || slot<0 || slot>=slotCaps[group] || joint<0 || joint>=jointCaps[group] ||
               !std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z) || p.x<0 || p.x>1 || p.y<0 || p.y>1 || p.z<0 || p.z>1 ||
               (active && t!=current.time) || (!active && t<=previous)) { startError="Replay: invalid record or timestamp"; return false; }
            active=true; current.time=t; p.group=static_cast<uint8_t>(group); p.id=uint32_t(slot*(group==kGroupFace?1000:100)+joint+(group==kGroupHandR?50:0)); p.confidence=p.z; p.state=TouchState::Move;
            for(uint16_t k=0;k<current.frame.count;++k) if(current.frame.points[k].id==p.id && current.frame.points[k].group==p.group) { startError="Replay: duplicate joint"; return false; }
            if(cfg.flip) p.x=1-p.x;
            if(!current.frame.addPoint(p)) { startError="Replay: too many points"; return false; }

        }
        if(!finish()) return false;
        if(file.bad() || replay.empty()) { startError="Replay: empty or unreadable file"; return false; } return true;
    }
    void publish(SensorFrame& f) { f.seq=++seq; f.sourceId=owner.sourceId(); owner.frames().write(f); }
    void capture() {
        bool opened=false; int64_t previous=0; std::string error;
        while(running) {
            if(!opened) {
                opened=device->open(cfg.cameraId,cfg.width,cfg.height,error);
                if(opened && !modeSupported(cfg.mode,device->caps(),error)) { device->close(); opened=false; }
                if(!opened) { message(captureError,"Camera: "+error); cameraOk=false; captureFailed=true; camFps=0; if(!wait(2)) break; continue; }
            }
            auto& f=captured.writeSlot();
            if(!device->read(f,100,error) || !valid(f)) {
                message(captureError,"Camera: "+(error.empty()?std::string("invalid RGB frame"):error)); cameraOk=false; captureFailed=true; camFps=0;
                device->close(); opened=false; previous=0; if(!wait(2)) break; continue;
            }
            auto now=nowNs(); if(previous) camFps=float(1e9/double(now-previous)); previous=now;
            captured.publish(); captureFailed=false; cameraOk=true;
        }
        if(opened) device->close(); cameraOk=false;
    }
    void infer() {
        if(seq) { SensorFrame reset; publish(reset); }
        PersonPose poses[30]; TrackedPerson tracks[30]; std::string error;
        int64_t lastArrival=0,previous=0; bool stale=false;
        while(running) {
            if(!captured.consume()) {
                if(lastArrival && !stale && double(nowNs()-lastArrival)*1e-9>=cfg.tracker.keepSec && !cameraOk) {
                    SensorFrame f; f.tCaptureNs=captured.readSlot().tCaptureNs; tracker.reset(); people=0; inferFps=0; publish(f); stale=true;
                }
                wait(.002); continue;
            }
            const auto& frame=captured.readSlot(); lastArrival=nowNs(); stale=false;
            SensorFrame f; f.tCaptureNs=frame.tCaptureNs;
            if(cfg.mode!=CamMode::Off && objectsCap>0) {
                error.clear(); const auto begin=nowNs();
                const int capacity=inference || cfg.mode==CamMode::Body ? 30 : objectsCap.load();
                int n=inference ? inference(frame,poses,capacity,error) : cfg.mode==CamMode::Body
                    ? model.infer(frame,cfg.personThr,cfg.jointThr,poses,capacity,error)
                    : topDown.infer(frame,cfg.personThr,cfg.jointThr,capacity,poses,error);
                if(n<0 || n>capacity) error="Invalid inference object count";
                if(error.empty()) for(int i=0;i<n;++i) {
                    if(poses[i].nkp!=modeSpec(cfg.mode).keypointsPerObject) { error="Inference keypoint count does not match mode"; break; }
                    if(cfg.mode==CamMode::Hand) poses[i].side=handSide(poses[i],cfg.flip);
                    for(int j=0;j<poses[i].nkp;++j) {
                        auto& k=poses[i].kp[j];
                        if(!std::isfinite(k.x) || !std::isfinite(k.y) || !std::isfinite(k.conf)) k={};
                        else { k.x=std::clamp(k.x,0.f,1.f); k.y=std::clamp(k.y,0.f,1.f); k.conf=std::clamp(k.conf,0.f,1.f); }
                    }
                }
                inferMs=float(double(nowNs()-begin)/1e6);
                if(n<0 || !error.empty()) { message(inferError,"Inference: "+error); inferOk=false; tracker.reset(); n=0; }
                else inferOk=true;
                int count=tracker.update(poses,std::clamp(n,0,30),double(lastArrival)*1e-9,tracks,30); people=count;
                for(int i=0;i<count;++i) for(int j=0;j<tracks[i].pose.nkp;++j) {
                    const auto& k=tracks[i].pose.kp[j]; if(k.conf<cfg.jointThr || k.conf<=0) continue;
                    TouchPoint p; bool emit=false;
                    pointGroupAndId(cfg.mode,tracks[i].slot,tracks[i].pose.side,j,p.group,p.id,emit);
                    if(!emit || (cfg.mode==CamMode::Full && !(cfg.fullGroups & (1u<<p.group)))) continue;
                    p.x=cfg.flip?1-k.x:k.x; p.y=k.y; p.z=p.confidence=k.conf; p.state=TouchState::Move; f.addPoint(p);
                }
                const auto done=nowNs(); if(previous) inferFps=float(1e9/double(done-previous)); previous=done;
                latencyMs=float(std::max(0.0,double(done-frame.tCaptureNs)/1e6));
            }
            preview(frame,previews.writeSlot(),cfg.flip); previews.publish();
            publish(f);
        }
    }
    void replayLoop() {
        if(seq) { SensorFrame reset; publish(reset); }
        cameraOk=true; const double step=replay.size()>1?replay.back().time-replay[replay.size()-2].time:1.0/30;
        const double duration=replay.back().time+step;
        auto origin=Clock::now(); int64_t previous=0;
        while(running) {
            for(const auto& r:replay) {
                auto due=origin+std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(r.time));
                while(running && Clock::now()<due) wait(.002);
                if(!running) return;
                auto f=r.frame; f.tCaptureNs=std::chrono::duration_cast<std::chrono::nanoseconds>(due.time_since_epoch()).count();
                uint16_t kept=0;
                for(uint16_t k=0;k<f.count;++k) {
                    const auto& point=f.points[k]; const auto slot=point.id/(point.group==kGroupFace?1000u:100u);
                    const bool groupEnabled=cfg.mode==CamMode::Full ? (cfg.fullGroups & (1u<<point.group))!=0 :
                        cfg.mode==CamMode::Body ? point.group==kGroupBody : cfg.mode==CamMode::Face ? point.group==kGroupFace :
                        cfg.mode==CamMode::Hand && (point.group==kGroupHandL || point.group==kGroupHandR);
                    if(groupEnabled && slot<static_cast<uint32_t>(objectsCap.load())) f.points[kept++]=point;
                }
                f.count=kept;
                auto& preview=previews.writeSlot(); preview.width=640; preview.height=480;
                preview.rgb.resize(640*480*3,73); preview.tCaptureNs=f.tCaptureNs; preview.index=seq+1;
                const auto arrival=nowNs(); if(previous) camFps=float(1e9/double(arrival-previous)); previous=arrival;
                latencyMs=float(double(arrival-f.tCaptureNs)*1e-6);
                bool slots[30]{}; int count=0; for(uint16_t k=0;k<f.count;++k) { const auto slot=f.points[k].id/(f.points[k].group==kGroupFace?1000u:100u); if(!slots[slot]) { slots[slot]=true; ++count; } }
                people=count; previews.publish(); publish(f);
            }
            origin+=std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(duration));
        }
    }
};
CameraSource::CameraSource(CameraSourceConfig c,std::unique_ptr<ICameraDevice> d,Inference i):impl_(std::make_unique<Impl>(*this,std::move(c),std::move(d),std::move(i))) {}
CameraSource::~CameraSource() { stop(); }
bool CameraSource::start() {
    auto& p=*impl_; if(p.running) return true;
    try {
        p.startError.clear(); p.cameraOk=false; p.captureFailed=false;
        p.captured.consume(); p.previews.consume(); p.captureError.consume(); p.inferError.consume(); m_frames.consume(); p.inferOk=true; p.onCpu=false; p.people=0; p.camFps=0; p.inferFps=0; p.inferMs=0; p.latencyMs=0; p.tracker.reset();
        p.objectsCap=clampObjects(p.cfg.mode,p.cfg.maxObjects); p.calPoint=calibPointId(p.cfg.mode);
        auto trackerParams=p.cfg.tracker; trackerParams.maxPeople=p.objectsCap.load(); p.tracker.setParams(trackerParams);
        if(!std::isfinite(p.cfg.personThr) || !std::isfinite(p.cfg.jointThr)) { p.startError="Invalid inference thresholds"; return false; }
        if(!modeSupported(p.cfg.mode,CameraCaps{true},p.startError)) return false;
        if(!std::isfinite(p.cfg.tracker.keepSec) || p.cfg.tracker.keepSec<0) p.cfg.tracker.keepSec=.5;
        if(!p.cfg.replayPath.empty()) { if(!p.loadReplay()) return false; }
        else {
            if(p.cfg.mode==CamMode::Body && !p.inference) {
                if(p.cfg.runtimeDir.empty() || p.cfg.modelPath.empty()) { p.startError="Needs pack: install camera body runtime and model"; return false; }
                bool cuda=false;
                if(!p.runtime.load(p.cfg.runtimeDir,p.startError) || !p.runtime.createSession(p.cfg.modelPath,p.cfg.device,p.startError,&cuda) || !p.model.init(p.runtime,p.startError)) return false;
                p.onCpu=!cuda;
            }
            if(p.cfg.mode!=CamMode::Off && p.cfg.mode!=CamMode::Body && !p.inference) {
                if(p.cfg.runtimeDir.empty() || p.cfg.detectorPath.empty() || p.cfg.keypointPath.empty()) { p.startError="Needs pack: install camera mode runtime, detector and keypoints"; return false; }
                bool detectorCuda=false,keypointCuda=false;
                if(!p.runtime.load(p.cfg.runtimeDir,p.startError) || !p.runtime.createSession(p.cfg.detectorPath,p.cfg.device,p.startError,&detectorCuda) ||
                   !p.keypointRuntime.load(p.cfg.runtimeDir,p.startError) || !p.keypointRuntime.createSession(p.cfg.keypointPath,p.cfg.device,p.startError,&keypointCuda) ||
                   !p.topDown.init(p.runtime,p.keypointRuntime,modeSpec(p.cfg.mode).keypointsPerObject,p.startError)) return false;
                p.onCpu=!detectorCuda || !keypointCuda;
                if(p.cfg.mode==CamMode::Face) {
                    std::string value,error; int index=-1;
                    if(p.keypointRuntime.customMetadata("mikmap.face.nose_tip_index",value,error) && !value.empty() &&
                       std::all_of(value.begin(),value.end(),[](char c){return c>='0' && c<='9';})) {
                        const auto result=std::from_chars(value.data(),value.data()+value.size(),index);
                        if(result.ec==std::errc{} && result.ptr==value.data()+value.size()) p.calPoint=calibPointId(p.cfg.mode,index);
                    }
                }
            }
            if(!p.device) p.device=std::make_unique<MfCamera>();
        }
        p.running=true;
        if(!p.cfg.replayPath.empty()) p.inferThread=std::thread([&p]{p.replayLoop();});
        else { p.captureThread=std::thread([&p]{ try { p.capture(); } catch(const std::exception& e) { message(p.captureError,std::string("Camera: ")+e.what()); p.cameraOk=false; p.captureFailed=true; p.device->close(); } }); p.inferThread=std::thread([&p]{ try { p.infer(); } catch(const std::exception& e) { message(p.inferError,std::string("Inference: ")+e.what()); p.inferOk=false; p.people=0; SensorFrame f; p.publish(f); } }); }
    } catch(const std::exception& e) { stop(); p.startError=e.what(); return false; }
    return true;
}
void CameraSource::stop() {
    auto& p=*impl_; p.running=false;
    if(p.captureThread.joinable()) p.captureThread.join();
    if(p.inferThread.joinable()) p.inferThread.join();
    p.cameraOk=false;
}
SourceStatus CameraSource::status() const {
    const auto& p=*impl_; if(!p.startError.empty()) return SourceStatus::Error;
    if(!p.running) return SourceStatus::Stopped;
    if(!p.inferOk) return SourceStatus::Error;
    if(!p.cameraOk) return p.captureFailed?SourceStatus::Error:SourceStatus::Connecting;
    return SourceStatus::Running;
}
std::string CameraSource::lastError() const {
    auto& p=*impl_; if(!p.startError.empty()) return p.startError;
    p.captureError.consume(); p.inferError.consume();
    if(p.running && !p.cameraOk) return p.captureError.readSlot().text.data();
    if(p.running && !p.inferOk) return p.inferError.readSlot().text.data();
    return p.onCpu?"Running on CPU; inference may be slow":std::string{};
}
bool CameraSource::previewLatest(CameraFrame& out) { auto& p=*impl_; if(!p.previews.consume()) return false; out=p.previews.readSlot(); return true; }
CameraStats CameraSource::stats() const { const auto& p=*impl_; return {p.camFps.load(),p.inferFps.load(),p.inferMs.load(),p.latencyMs.load(),p.people.load(),p.onCpu.load(),p.objectsCap.load(),p.calPoint.load()}; }
}
