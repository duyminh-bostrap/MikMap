#pragma once
#include "PoseModel.h"
namespace mikmap {
struct CropXf { float a[6]={}; float inv[6]={}; int outW=0,outH=0; };
bool makeCrop(const float box[4],int imgW,int imgH,int outW,int outH,float padding,CropXf& out);
void warpCropToChw(const CameraFrame&,const CropXf&,std::vector<float>& outChw);
void decodeSimcc(const float* sx,const float* sy,int K,int outW,int outH,float split,const CropXf&,int imgW,int imgH,Keypoint* out);
struct DetBox { float box[4]={}; float score=0; }; // Image pixels, xyxy.
Letterbox letterboxRtmDetToChw(const CameraFrame&,int inW,int inH,std::vector<float>& outChw);
int decodeDetector(const float* dets,int n,const Letterbox&,int imgW,int imgH,float thr,DetBox* out,int maxOut);
// End-to-end RTMDet requires int64 labels; mask non-target classes before geometric decoding.
bool filterDetectorLabels(std::vector<float>& dets,const std::vector<int64_t>& labels,
                          const std::vector<int64_t>& detShape,const std::vector<int64_t>& labelShape,std::string& err);
// rtmlib BGR preprocessing, MMDeploy end-to-end dets/labels, SimCC split 2.
// Class 0 means person for COCO; hand/face packs require target-specific class 0 detectors.
class TopDownPose {
public:
    bool init(OrtRuntime& detector,OrtRuntime& keypoints,int numKeypoints,std::string& err);
    int infer(const CameraFrame&,float detThr,float kpThr,int maxObjects,PoseObject* out,std::string& err);
private:
    OrtRuntime* detector_=nullptr; OrtRuntime* keypoints_=nullptr;
    int nkp_=0; size_t detIndex_=0,labelIndex_=0,xIndex_=0,yIndex_=0;
    std::vector<int64_t> detShape_,kpShape_;
    std::vector<float> detInput_,kpInput_;
    std::vector<std::vector<float>> detOutputs_,kpOutputs_;
    std::vector<std::vector<int64_t>> labels_,detShapes_,kpShapes_;
    std::vector<DetBox> boxes_;
};
}
