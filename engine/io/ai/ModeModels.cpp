#include "ModeModels.h"
#include "io/SensorFrame.h"
#include <algorithm>
#include <cmath>

namespace mikmap {
const ModeSpec& modeSpec(CamMode mode) {
    static constexpr ModeSpec specs[]={
        {CamMode::Off,"","","",0,0},
        {CamMode::Body,"pose-rtmo","","rtmo.onnx",17,30},
        {CamMode::Hand,"hand-rtm","detector.onnx","keypoints.onnx",21,24},
        {CamMode::Face,"face-rtm","detector.onnx","keypoints.onnx",106,4},
        {CamMode::Full,"wholebody-rtmw","detector.onnx","keypoints.onnx",133,3}
    };
    const int i=static_cast<int>(mode);
    return specs[i>=0 && i<5 ? i : 0];
}
int clampObjects(CamMode mode,int requested) {
    return std::clamp(requested,0,modeSpec(mode).objectsCap);
}
void pointGroupAndId(CamMode mode,int slot,int8_t side,int k,uint8_t& group,uint32_t& id,bool& emit) {
    emit=false;
    const auto& spec=modeSpec(mode);
    if(slot<0 || slot>=spec.objectsCap || k<0 || k>=spec.keypointsPerObject) return;
    uint8_t g=kGroupBody;
    int index=k;
    switch(mode) {
    case CamMode::Body: index=kCocoToMediaPipe[k]; break;
    case CamMode::Hand:
        if(side!=0 && side!=1) return;
        g=side==0 ? kGroupHandL : kGroupHandR;
        break;
    case CamMode::Face: g=kGroupFace; break;
    case CamMode::Full:
        if(k<17) index=kCocoToMediaPipe[k];
        else if(k<23) { g=kGroupFeet; index=k-17; }
        else if(k<91) { g=kGroupFace; index=k-23; }
        else if(k<112) { g=kGroupHandL; index=k-91; }
        else { g=kGroupHandR; index=k-112; }
        break;
    default: return;
    }
    group=g;
    id=static_cast<uint32_t>(slot*(g==kGroupFace ? 1000 : 100)+index+(g==kGroupHandR ? 50 : 0));
    emit=true;
}
uint32_t calibPointId(CamMode mode,int faceNoseIndex) {
    switch(mode) {
    case CamMode::Body: case CamMode::Full: return 16;
    case CamMode::Hand: return 58;
    // LaPa106 has no verified default nose index; the model publisher must supply verified metadata.
    case CamMode::Face: return faceNoseIndex>=0 && faceNoseIndex<106 ? static_cast<uint32_t>(faceNoseIndex) : kCalibUnavailable;
    default: return kCalibUnavailable;
    }
}
int8_t handSide(const PoseObject& hand,bool flipped) {
    if(hand.nkp!=21) return -1;
    for(int k:{0,5,17}) {
        const auto& p=hand.kp[k];
        if(!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.conf) || p.conf<=0) return -1;
    }
    const auto& w=hand.kp[0]; const auto& i=hand.kp[5]; const auto& p=hand.kp[17];
    const double cross=(double(i.x)-w.x)*(double(p.y)-w.y)-(double(i.y)-w.y)*(double(p.x)-w.x);
    if(cross==0) return -1;
    return static_cast<int8_t>((cross>0)!=flipped);
}
}
