#include "TestHarness.h"
#include "io/ai/PoseModel.h"
#include <limits>
#include <cstdlib>
#include <iostream>
using namespace mikmap;
TEST_CASE("RTMO COCO map and top-left letterbox") {
    const int expected[] = {0,2,5,7,8,11,12,13,14,15,16,23,24,25,26,27,28};
    for (int i=0;i<17;++i) CHECK(kCocoToMediaPipe[i]==expected[i]);
    CHECK(kCocoToMediaPipe[10]==16);
    CameraFrame f; f.width=1280; f.height=720; f.rgb.resize(1280*720*3, 7);
    std::vector<float> chw; auto lb=letterboxRgbToChw(f,640,640,chw);
    CHECK_NEAR(lb.scale,0.5,1e-6); CHECK(lb.padX==0); CHECK(lb.padY==0);
    CHECK(chw[0]==7); CHECK(chw[360*640]==114);
    auto* data=chw.data(); letterboxRgbToChw(f,640,640,chw); CHECK(data==chw.data());
    f.width=2; f.height=1; f.rgb={10,20,30,110,120,130};
    letterboxRgbToChw(f,4,2,chw);
    CHECK(chw[0]==30); CHECK(chw[1]==55); CHECK(chw[8]==20); CHECK(chw[16]==10);
}
TEST_CASE("RTMO decode filters sorts caps and undoes padding") {
    float dets[]={0,0,200,360,.7f, 0,0,200,360,.1f, 300,0,640,360,.9f};
    float kps[3*17*3]; for(int i=0;i<51;++i) { kps[i*3]=320; kps[i*3+1]=180; kps[i*3+2]=.8f; }
    kps[2*51+2]=.1f;
    Letterbox lb{.5f,0,0,640,640}; PersonPose out[3];
    REQUIRE(decodeRtmo(dets,kps,3,lb,1280,720,.5f,.5f,out,3)==2);
    CHECK_NEAR(out[0].score,.9,1e-6); CHECK_NEAR(out[1].score,.7,1e-6);
    CHECK_NEAR(out[0].kp[1].x,.5,1e-4); CHECK_NEAR(out[0].kp[1].y,.5,1e-4);
    CHECK(out[0].kp[0].conf==0); CHECK(out[0].box[3]==1);
    REQUIRE(decodeRtmo(dets,kps,3,lb,1280,720,.5f,.5f,out,1)==1); CHECK_NEAR(out[0].score,.9,1e-6);
    CHECK(decodeRtmo(nullptr,nullptr,0,lb,1280,720,.5f,.5f,out,3)==0);
    CHECK(decodeRtmo(nullptr,kps,3,lb,1280,720,.5f,.5f,out,3)==0);
    lb.scale=0; CHECK(decodeRtmo(dets,kps,3,lb,1280,720,.5f,.5f,out,3)==0);
    lb.scale=.5f; dets[14]=std::numeric_limits<float>::quiet_NaN();
    CHECK(decodeRtmo(dets,kps,3,lb,1280,720,.5f,.5f,out,3)==1);
}
TEST_CASE("RTMO rejects uninitialized session and malformed frame") {
    OrtRuntime rt; PoseModel model; std::string err;
    CHECK(!model.init(rt,err)); CHECK(!err.empty());
    CameraFrame f; PersonPose out[1]; CHECK(model.infer(f,.5f,.5f,out,1,err)==0); CHECK(!err.empty());
    std::vector<float> chw; CHECK(letterboxRgbToChw(f,640,640,chw).scale==0); CHECK(chw.empty());
}

TEST_CASE("RTMO NMS retains strongest overlapping detection and handles nonfinite joints") {
    float dets[]={0,0,640,640,.7f, 0,0,640,640,.9f}; float kps[102]={}; PersonPose out[2];
    kps[51]=std::numeric_limits<float>::infinity(); kps[53]=.9f;
    REQUIRE(decodeRtmo(dets,kps,2,Letterbox{},640,640,.5f,.5f,out,2)==1);
    CHECK_NEAR(out[0].score,.9,1e-6); CHECK(out[0].kp[0].conf==0);
}
TEST_CASE("RTMO optional real model gray frame inference") {
    const auto env=[](const char* key) {
#ifdef _MSC_VER
        char* raw=nullptr; size_t length=0; if(_dupenv_s(&raw,&length,key)!=0) return std::string{};
        std::string value=raw?raw:""; std::free(raw); return value;
#else
        const char* raw=std::getenv(key); return std::string(raw?raw:"");
#endif
    };
    const auto dir=env("MIKMAP_TEST_ORT_DIR"),path=env("MIKMAP_TEST_POSE_MODEL");
    if(dir.empty()||path.empty()) { std::cout<<"[SKIP] RTMO gray-frame inference: set MIKMAP_TEST_ORT_DIR and MIKMAP_TEST_POSE_MODEL\n"; return; }
    OrtRuntime rt; PoseModel model; std::string err;
    REQUIRE(rt.load(dir,err)); REQUIRE(rt.createSession(path,OrtDevice::Cpu,err)); REQUIRE(model.init(rt,err));
    CameraFrame f; f.width=640; f.height=640; f.rgb.resize(640*640*3,128); PersonPose poses[16];
    CHECK(model.infer(f,.7f,.5f,poses,16,err)==0); CHECK(err.empty());
}
