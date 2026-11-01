#include "TestHarness.h"
#include "io/ai/ModeModels.h"
#include "io/ai/PersonTracker.h"
#include "io/SensorFrame.h"
#include <limits>
#include <string>
using namespace mikmap;

TEST_CASE("Mode models table caps and calibration metadata") {
    const CamMode modes[]={CamMode::Off,CamMode::Body,CamMode::Hand,CamMode::Face,CamMode::Full};
    const int counts[]={0,17,21,106,133}, caps[]={0,30,24,4,3};
    const char* packs[]={"","pose-rtmo","hand-rtm","face-rtm","wholebody-rtmw"};
    for(int i=0;i<5;++i) {
        const auto& s=modeSpec(modes[i]); CHECK(s.mode==modes[i]);
        CHECK(s.keypointsPerObject==counts[i]); CHECK(s.objectsCap==caps[i]); CHECK(std::string(s.pack)==packs[i]);
        CHECK(clampObjects(modes[i],100)==caps[i]); CHECK(clampObjects(modes[i],2)==(i?2:0));
        CHECK(clampObjects(modes[i],0)==0); CHECK(clampObjects(modes[i],-1)==0);
        CHECK(clampObjects(modes[i],std::numeric_limits<int>::max())==caps[i]);
        CHECK(std::string(s.detectorFile)==(i>=2?"detector.onnx":""));
        CHECK(std::string(s.keypointFile)==(i>=2?"keypoints.onnx":i==1?"rtmo.onnx":""));
    }
    CHECK(calibPointId(CamMode::Body)==16u); CHECK(calibPointId(CamMode::Full)==16u);
    CHECK(calibPointId(CamMode::Hand)==58u); CHECK(calibPointId(CamMode::Off)==kCalibUnavailable);
    CHECK(calibPointId(CamMode::Face)==kCalibUnavailable); CHECK(calibPointId(CamMode::Face,7)==7u);
    CHECK(calibPointId(CamMode::Face,0)==0u); CHECK(calibPointId(CamMode::Face,105)==105u);
    CHECK(calibPointId(CamMode::Face,106)==kCalibUnavailable); CHECK(calibPointId(CamMode::Face,-2)==kCalibUnavailable);
    CHECK(modeSpec(static_cast<CamMode>(99)).mode==CamMode::Off);
    CHECK(clampObjects(static_cast<CamMode>(99),1)==0);
    CHECK(calibPointId(static_cast<CamMode>(99))==kCalibUnavailable);
}

TEST_CASE("Mode point groups map every full-body boundary and reject invalid inputs") {
    uint8_t group=99; uint32_t id=999; bool emit=false;
    const int keys[]={0,10,16,17,22,23,90,91,111,112,120,132};
    const uint8_t groups[]={0,0,0,4,4,3,3,1,1,2,2,2};
    const uint32_t ids[]={100,116,128,100,105,1000,1067,100,120,150,158,170};
    for(int i=0;i<12;++i) { pointGroupAndId(CamMode::Full,1,-1,keys[i],group,id,emit); CHECK(emit); CHECK(group==groups[i]); CHECK(id==ids[i]); }
    for(int k=0;k<17;++k) { pointGroupAndId(CamMode::Body,29,-1,k,group,id,emit); CHECK(emit); CHECK(group==kGroupBody); CHECK(id==uint32_t(2900+kCocoToMediaPipe[k])); }
    pointGroupAndId(CamMode::Face,3,-1,105,group,id,emit); CHECK(emit); CHECK(group==kGroupFace); CHECK(id==3105u);
    for(int side=0;side<2;++side) for(int k=0;k<21;++k) { pointGroupAndId(CamMode::Hand,0,static_cast<int8_t>(side),k,group,id,emit); CHECK(emit); CHECK(group==(side?kGroupHandR:kGroupHandL)); CHECK(id==uint32_t(side*50+k)); }
    for(CamMode m:{CamMode::Off,CamMode::Body,CamMode::Hand,CamMode::Face,CamMode::Full,static_cast<CamMode>(99)}) {
        for(int slot:{-1,modeSpec(m).objectsCap,std::numeric_limits<int>::max()}) { pointGroupAndId(m,slot,0,0,group,id,emit); CHECK(!emit); }
        for(int k:{-1,modeSpec(m).keypointsPerObject,std::numeric_limits<int>::max()}) { pointGroupAndId(m,0,0,k,group,id,emit); CHECK(!emit); }
    }
    for(int8_t side:{int8_t(-1),int8_t(2)}) { pointGroupAndId(CamMode::Hand,0,side,8,group,id,emit); CHECK(!emit); }
}

TEST_CASE("Hand geometry and shared tracker keep side through a single disagreement") {
    PoseObject hand; hand.nkp=21; hand.score=1;
    hand.kp[0]={.5f,.8f,1}; hand.kp[5]={.3f,.4f,1}; hand.kp[17]={.7f,.4f,1};
    CHECK(handSide(hand,false)==1); CHECK(handSide(hand,true)==0);
    PoseObject mirrored=hand;
    for(int k=0;k<21;++k) mirrored.kp[k].x=1-hand.kp[k].x;
    CHECK(handSide(mirrored,false)==0); CHECK(handSide(mirrored,true)==1);
    PersonTracker tracker; TrackedPerson out[30];
    for(int f=0;f<5;++f) { hand.side=handSide(f==3?mirrored:hand,false); REQUIRE(tracker.update(&hand,1,f*.05,out,30)==1); CHECK(out[0].pose.side==1); }
    hand.kp[5].conf=0; CHECK(handSide(hand,false)==-1); hand.kp[5].conf=1;
    hand.kp[5]=hand.kp[17]; CHECK(handSide(hand,false)==-1);
    hand.kp[5].x=std::numeric_limits<float>::quiet_NaN(); CHECK(handSide(hand,false)==-1);
    hand.nkp=17; CHECK(handSide(hand,false)==-1);
}
