#include "TestHarness.h"
#include "io/ai/TopDownPose.h"
#include <cmath>
#include <limits>
#include <cstdlib>
#include <iostream>
#include <filesystem>
using namespace mikmap;
TEST_CASE("Top-down crop preserves padded aspect and affine inverse") {
    float box[]={10,20,110,220}; CropXf xf;
    REQUIRE(makeCrop(box,640,480,192,256,1.25f,xf));
    CHECK_NEAR(xf.a[0],1.024,1e-6); CHECK_NEAR(xf.a[4],1.024,1e-6);
    CHECK_NEAR(xf.a[0]*60+xf.a[2],96,1e-4);
    CHECK_NEAR(xf.a[4]*120+xf.a[5],128,1e-4);
    for(float p : {0.f,37.f,640.f}) {
        CHECK_NEAR(xf.inv[0]*(xf.a[0]*p+xf.a[2])+xf.inv[2],p,1e-4);
        CHECK_NEAR(xf.inv[4]*(xf.a[4]*p+xf.a[5])+xf.inv[5],p,1e-4);
    }
    float edge[]={-20,-30,20,50}; CHECK(makeCrop(edge,640,480,192,256,1.25f,xf));
    box[2]=box[0]; CHECK(!makeCrop(box,640,480,192,256,1.25f,xf));
    box[2]=110; box[0]=std::numeric_limits<float>::quiet_NaN(); CHECK(!makeCrop(box,640,480,192,256,1.25f,xf));
    box[0]=std::numeric_limits<float>::infinity(); CHECK(!makeCrop(box,640,480,192,256,1.25f,xf));
    box[0]=10; CHECK(!makeCrop(box,0,480,192,256,1.25f,xf));
    CHECK(!makeCrop(box,640,480,192,256,0,xf));
}
TEST_CASE("Top-down crop samples bilinear BGR and black before normalization") {
    CameraFrame f; f.width=2; f.height=1; f.rgb={10,20,30,110,120,130};
    CropXf xf; xf.outW=3; xf.outH=1; xf.inv[0]=.5f; xf.inv[4]=1; xf.inv[2]=-.5f;
    std::vector<float> out; warpCropToChw(f,xf,out); REQUIRE(out.size()==9);
    CHECK_NEAR(out[0],(15-123.675)/58.395,1e-5);
    CHECK_NEAR(out[1],(30-123.675)/58.395,1e-5);
    CHECK_NEAR(out[2],(80-123.675)/58.395,1e-5);
    CHECK_NEAR(out[4],(20-116.28)/57.12,1e-5);
    CHECK_NEAR(out[7],(10-103.53)/57.375,1e-5);
    auto* data=out.data(); warpCropToChw(f,xf,out); CHECK(data==out.data());
    xf.inv[2]=-10; warpCropToChw(f,xf,out); CHECK_NEAR(out[0],-123.675/58.395,1e-5);
    xf.inv[2]=std::numeric_limits<float>::infinity(); warpCropToChw(f,xf,out); CHECK(out.empty());
    xf.inv[2]=0; f.rgb.clear(); warpCropToChw(f,xf,out); CHECK(out.empty());
}
TEST_CASE("SimCC peaks invert crop and reject nonfinite scores") {
    float box[]={10,20,110,220}; CropXf xf; REQUIRE(makeCrop(box,640,480,192,256,1.25f,xf));
    std::vector<float> sx(2*384),sy(2*512); sx[192]=.8f; sy[256]=.6f; Keypoint kp[2];
    decodeSimcc(sx.data(),sy.data(),2,192,256,2,xf,640,480,kp);
    CHECK_NEAR(kp[0].x,60./640,1e-5); CHECK_NEAR(kp[0].y,120./480,1e-5); CHECK_NEAR(kp[0].conf,.6,1e-6);
    CHECK(kp[1].conf==0); CHECK(std::isfinite(kp[1].x)); CHECK(std::isfinite(kp[1].y));
    sx[192]=std::numeric_limits<float>::infinity();
    decodeSimcc(sx.data(),sy.data(),2,192,256,2,xf,640,480,kp); CHECK(kp[0].conf==0);
    sx[192]=2; sy[256]=3; decodeSimcc(sx.data(),sy.data(),2,192,256,2,xf,640,480,kp); CHECK(kp[0].conf==1);
}
TEST_CASE("RTMDet geometry restores pixel boxes sorted capped and clipped") {
    float dets[]={0,0,50,100,.7f, 100,50,200,150,.9f, 0,0,10,10,.1f, -50,-50,-1,-1,1};
    DetBox boxes[4]; Letterbox lb{.5f,0,0,320,240};
    REQUIRE(decodeDetector(dets,4,lb,640,480,.5f,boxes,4)==2);
    CHECK_NEAR(boxes[0].score,.9,1e-6); CHECK_NEAR(boxes[0].box[0],200,1e-6); CHECK_NEAR(boxes[0].box[3],300,1e-6);
    CHECK(decodeDetector(dets,4,lb,640,480,.5f,boxes,1)==1);
    lb.scale=0; CHECK(decodeDetector(dets,4,lb,640,480,.5f,boxes,4)==0);
}
TEST_CASE("RTMDet labels validate tensor layout and keep only class zero") {
    std::vector<float> dets={0,0,10,10,.8f, 10,10,20,20,.9f}; std::string err;
    REQUIRE(filterDetectorLabels(dets,{0,17},{1,2,5},{1,2},err));
    DetBox boxes[2]; REQUIRE(decodeDetector(dets.data(),2,Letterbox{},640,640,.5f,boxes,2)==1); CHECK_NEAR(boxes[0].score,.8,1e-6);
    CHECK(!filterDetectorLabels(dets,{0},{1,2,5},{1,1},err)); CHECK(!err.empty());
    CHECK(!filterDetectorLabels(dets,{0,0},{2,5},{1,2},err));
    CHECK(!filterDetectorLabels(dets,{0,0},{1,2,5},{2},err));
    CHECK(!filterDetectorLabels(dets,{0,0},{1,std::numeric_limits<int64_t>::max(),5},{1,2},err));
    std::vector<float> empty; CHECK(filterDetectorLabels(empty,{}, {1,0,5},{1,0},err));
}
TEST_CASE("Top-down sessions require initialization") {
    OrtRuntime det,kp; TopDownPose model; std::string err;
    CHECK(!model.init(det,kp,21,err)); CHECK(!err.empty());
    CameraFrame frame; PoseObject out[1]; CHECK(model.infer(frame,.5f,.5f,1,out,err)==0); CHECK(!err.empty());
}
TEST_CASE("RTMDet normalization preserves BGR order and normalizes letterbox padding") {
    CameraFrame f; f.width=2; f.height=1; f.rgb={10,20,30,10,20,30}; std::vector<float> chw;
    const auto lb=letterboxRtmDetToChw(f,2,2,chw); REQUIRE(chw.size()==12); CHECK(lb.scale==1);
    CHECK_NEAR(chw[0],(30-103.53)/57.375,1e-5);
    CHECK_NEAR(chw[4],(20-116.28)/57.12,1e-5);
    CHECK_NEAR(chw[8],(10-123.675)/58.395,1e-5);
    CHECK_NEAR(chw[2],(114-103.53)/57.375,1e-5);
    auto* data=chw.data(); letterboxRtmDetToChw(f,2,2,chw); CHECK(data==chw.data());
    f.rgb.clear(); CHECK(letterboxRtmDetToChw(f,2,2,chw).scale==0); CHECK(chw.empty());
}
TEST_CASE("Top-down optional hand detector typed outputs and gray frame inference") {
    const auto env=[](const char* key) {
#ifdef _MSC_VER
        char* raw=nullptr; size_t length=0; if(_dupenv_s(&raw,&length,key)!=0) return std::string{};
        std::string value=raw?raw:""; std::free(raw); return value;
#else
        const char* raw=std::getenv(key); return std::string(raw?raw:"");
#endif
    };
    const auto dir=env("MIKMAP_TEST_ORT_DIR"),models=env("MIKMAP_TEST_HAND_MODELS");
    if(dir.empty()||models.empty()) { std::cout<<"[SKIP] Top-down hand inference: set MIKMAP_TEST_ORT_DIR and MIKMAP_TEST_HAND_MODELS=detector.onnx;keypoints.onnx\n"; return; }
    const auto separator=models.find(';'); REQUIRE(separator!=std::string::npos); REQUIRE(separator>0); REQUIRE(separator+1<models.size()); REQUIRE(models.find(';',separator+1)==std::string::npos);
    OrtRuntime detector,keypoints; TopDownPose model; std::string err;
    REQUIRE(detector.load(dir,err)); REQUIRE(keypoints.load(dir,err));
    REQUIRE(detector.createSession(models.substr(0,separator),OrtDevice::Cpu,err));
    REQUIRE(keypoints.createSession(models.substr(separator+1),OrtDevice::Cpu,err)); REQUIRE(model.init(detector,keypoints,21,err));
    CameraFrame f; f.width=640; f.height=480; f.rgb.resize(640*480*3,128);
    const auto shape=detector.inputs()[0].shape; std::vector<float> input;
    REQUIRE(letterboxRtmDetToChw(f,static_cast<int>(shape[3]),static_cast<int>(shape[2]),input).scale>0);
    std::vector<std::vector<float>> outputs; std::vector<std::vector<int64_t>> integers,shapes;
    CHECK(!detector.run(input.data(),shape,outputs,shapes,err)); CHECK(err.find("typed")!=std::string::npos);
    REQUIRE(detector.run(input.data(),shape,outputs,integers,shapes,err));
    const auto info=detector.outputs(); size_t di=info.size(),li=info.size();
    for(size_t i=0;i<info.size();++i) { if(info[i].name=="dets") di=i; if(info[i].name=="labels") li=i; }
    REQUIRE(di<info.size()); REQUIRE(li<info.size()); CHECK(info[li].type==OrtTensorType::Int64);
    CHECK(outputs[li].empty()); CHECK(integers[di].empty()); REQUIRE(filterDetectorLabels(outputs[di],integers[li],shapes[di],shapes[li],err));
    PoseObject out[24]; CHECK(model.infer(f,.7f,.5f,24,out,err)==0); CHECK(err.empty());
}
TEST_CASE("Top-down optional constant ONNX fixtures exercise typed labels and every keypoint count") {
    const auto env=[](const char* key) {
#ifdef _MSC_VER
        char* raw=nullptr; size_t length=0; if(_dupenv_s(&raw,&length,key)!=0) return std::string{};
        std::string value=raw?raw:""; std::free(raw); return value;
#else
        const char* raw=std::getenv(key); return std::string(raw?raw:"");
#endif
    };
    const auto runtime=env("MIKMAP_TEST_ORT_DIR"),dir=env("MIKMAP_TEST_TOPDOWN_FIXTURES");
    if(runtime.empty()||dir.empty()) { std::cout<<"[SKIP] Constant ONNX fixtures: set MIKMAP_TEST_ORT_DIR and MIKMAP_TEST_TOPDOWN_FIXTURES\n"; return; }
    const auto path=[&](const std::string& name) { const auto p=(std::filesystem::path(std::u8string(dir.begin(),dir.end()))/name).u8string(); return std::string(p.begin(),p.end()); };
    OrtRuntime detector; std::string err; REQUIRE(detector.load(runtime,err)); REQUIRE(detector.createSession(path("detector.onnx"),OrtDevice::Cpu,err));
    CameraFrame f; f.width=640; f.height=480; f.rgb.resize(640*480*3,128); std::vector<float> input;
    REQUIRE(letterboxRtmDetToChw(f,640,640,input).scale==1);
    std::vector<std::vector<float>> outputs; std::vector<std::vector<int64_t>> integers,shapes;
    REQUIRE(detector.run(input.data(),{1,3,640,640},outputs,integers,shapes,err));
    REQUIRE(integers.size()==2); REQUIRE(integers[1].size()==4); CHECK(integers[1][0]==1099511627793LL); CHECK(outputs[1].empty());
    CHECK(!detector.run(input.data(),{1,3,640,640},outputs,shapes,err)); CHECK(err.find("typed")!=std::string::npos);
    for(int k : {21,106,133}) {
        OrtRuntime keypoints; TopDownPose model; REQUIRE(keypoints.load(runtime,err)); REQUIRE(keypoints.createSession(path("keypoints-"+std::to_string(k)+".onnx"),OrtDevice::Cpu,err)); REQUIRE(model.init(detector,keypoints,k,err));
        PoseObject out[3]; REQUIRE(model.infer(f,.5f,.5f,3,out,err)==2); CHECK(err.empty()); CHECK(out[0].nkp==k); CHECK(out[1].nkp==k);
        CHECK_NEAR(out[0].score,.9,1e-6); CHECK_NEAR(out[1].score,.8,1e-6); CHECK_NEAR(out[0].box[0],100./640,1e-6);
        CHECK_NEAR(out[0].kp[k-1].x,150./640,1e-6); CHECK_NEAR(out[0].kp[k-1].y,200./480,1e-6); CHECK_NEAR(out[0].kp[k-1].conf,.6,1e-6);
        REQUIRE(model.infer(f,.5f,.7f,1,out,err)==1); CHECK(out[0].kp[k-1].conf==0);
        CHECK(model.infer(f,.5f,.5f,0,out,err)==0); CHECK(!err.empty());
    }
}
