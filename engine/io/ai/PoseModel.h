#pragma once
#include "OrtRuntime.h"
#include "io/camera/CameraDevice.h"
#include <cstdint>
namespace mikmap {
struct Keypoint { float x=0,y=0,conf=0; };
inline constexpr int kMaxKeypoints=133;
// Default Body layout preserves existing PersonPose callers; other models set nkp explicitly.
struct PoseObject { float box[4]={}; float score=0; int nkp=17; int8_t side=-1; /* -1 unknown, 0 left, 1 right */ Keypoint kp[kMaxKeypoints]; };
using PersonPose=PoseObject;
struct Letterbox { float scale=1,padX=0,padY=0; int inW=640,inH=640; };
// Invalid frame/dimensions return scale=0 and clear outChw. Reuses its capacity.
Letterbox letterboxRgbToChw(const CameraFrame&,int inW,int inH,std::vector<float>& outChw);
int decodeRtmo(const float* dets,const float* kps,int n,const Letterbox&,int imgW,int imgH,
               float personThr,float jointThr,PersonPose* out,int maxOut);
extern const int kCocoToMediaPipe[17];
class PoseModel {
public:
    bool init(OrtRuntime&,std::string& err);
    int infer(const CameraFrame&,float personThr,float jointThr,PersonPose* out,int maxOut,std::string& err);
private:
    OrtRuntime* rt_=nullptr;
    std::vector<int64_t> shape_;
    std::vector<float> input_;
    std::vector<std::vector<float>> outputs_;
    std::vector<std::vector<int64_t>> outputShapes_;
    size_t detIndex_=0,kpIndex_=0;
};
}
