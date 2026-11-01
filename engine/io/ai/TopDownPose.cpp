#include "TopDownPose.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
namespace mikmap {
namespace {
bool pixels(int w,int h,size_t& count) {
    if(w<=0||h<=0||static_cast<size_t>(w)>std::numeric_limits<size_t>::max()/sizeof(float)/3/static_cast<size_t>(h)) return false;
    count=static_cast<size_t>(w)*static_cast<size_t>(h); return true;
}
bool finiteMatrix(const float* a) {
    for(int i=0;i<6;++i) if(!std::isfinite(a[i])) return false;
    return true;
}
bool inputShape(const std::vector<OrtTensorInfo>& in,std::vector<int64_t>& shape) {
    if(in.size()!=1||in[0].type!=OrtTensorType::Float32||in[0].shape.size()!=4) return false;
    const auto& s=in[0].shape;
    if(s[0]!=1||s[1]!=3||s[2]<=0||s[3]<=0||s[2]>std::numeric_limits<int>::max()/2||s[3]>std::numeric_limits<int>::max()/2) return false;
    size_t count=0; if(!pixels(static_cast<int>(s[3]),static_cast<int>(s[2]),count)) return false;
    shape=s; return true;
}
bool simccShape(const std::vector<int64_t>& s,int k,int bins) {
    return s.size()==3&&s[0]==1&&s[1]==k&&s[2]==bins;
}
}
bool makeCrop(const float* box,int imgW,int imgH,int outW,int outH,float padding,CropXf& out) {
    out={}; size_t count=0;
    if(!box||imgW<=0||imgH<=0||!pixels(outW,outH,count)||!std::isfinite(padding)||padding<=0) return false;
    for(int i=0;i<4;++i) if(!std::isfinite(box[i])) return false;
    double w=static_cast<double>(box[2])-box[0],h=static_cast<double>(box[3])-box[1];
    if(w<=0||h<=0) return false;
    const double cx=(static_cast<double>(box[0])+box[2])*.5,cy=(static_cast<double>(box[1])+box[3])*.5;
    const double aspect=static_cast<double>(outW)/outH;
    if(w>h*aspect) h=w/aspect; else w=h*aspect;
    w*=padding; h*=padding;
    CropXf xf; xf.outW=outW; xf.outH=outH;
    xf.a[0]=static_cast<float>(outW/w); xf.a[4]=static_cast<float>(outH/h);
    xf.a[2]=static_cast<float>(outW*.5-cx*outW/w); xf.a[5]=static_cast<float>(outH*.5-cy*outH/h);
    xf.inv[0]=static_cast<float>(w/outW); xf.inv[4]=static_cast<float>(h/outH);
    xf.inv[2]=static_cast<float>(cx-w*.5); xf.inv[5]=static_cast<float>(cy-h*.5);
    if(!finiteMatrix(xf.a)||!finiteMatrix(xf.inv)||xf.a[0]<=0||xf.a[4]<=0||xf.inv[0]<=0||xf.inv[4]<=0) return false;
    out=xf; return true;
}
void warpCropToChw(const CameraFrame& f,const CropXf& xf,std::vector<float>& out) {
    size_t src=0,dst=0;
    if(!pixels(f.width,f.height,src)||!pixels(xf.outW,xf.outH,dst)||f.rgb.size()!=src*3||dst>out.max_size()/3||!finiteMatrix(xf.inv)) { out.clear(); return; }
    out.resize(dst*3);
    // rtmlib RTMPose normalizes BGR directly, including the black affine border.
    constexpr float mean[]={123.675f,116.28f,103.53f},stddev[]={58.395f,57.12f,57.375f};
    for(int y=0;y<xf.outH;++y) for(int x=0;x<xf.outW;++x) {
        const double sx=static_cast<double>(xf.inv[0])*x+static_cast<double>(xf.inv[1])*y+xf.inv[2];
        const double sy=static_cast<double>(xf.inv[3])*x+static_cast<double>(xf.inv[4])*y+xf.inv[5];
        float bgr[3]={};
        if(sx>-1&&sy>-1&&sx<f.width&&sy<f.height) {
            const int x0=static_cast<int>(std::floor(sx)),y0=static_cast<int>(std::floor(sy));
            const float fx=static_cast<float>(sx-x0),fy=static_cast<float>(sy-y0);
            for(int c=0;c<3;++c) {
                const auto at=[&](int px,int py) { return px<0||py<0||px>=f.width||py>=f.height?0.f:static_cast<float>(f.rgb[(static_cast<size_t>(py)*f.width+px)*3+2-c]); };
                bgr[c]=(at(x0,y0)*(1-fx)+at(x0+1,y0)*fx)*(1-fy)+(at(x0,y0+1)*(1-fx)+at(x0+1,y0+1)*fx)*fy;
            }
        }
        for(int c=0;c<3;++c) out[static_cast<size_t>(c)*dst+static_cast<size_t>(y)*xf.outW+x]=(bgr[c]-mean[c])/stddev[c];
    }
}
void decodeSimcc(const float* sx,const float* sy,int K,int outW,int outH,float split,const CropXf& xf,int imgW,int imgH,Keypoint* out) {
    if(!out||K<=0||K>kMaxKeypoints) return;
    std::fill_n(out,K,Keypoint{});
    const double nx=static_cast<double>(outW)*split,ny=static_cast<double>(outH)*split;
    if(!sx||!sy||imgW<=0||imgH<=0||outW<=0||outH<=0||!std::isfinite(split)||split<=0||nx<1||ny<1||nx>std::numeric_limits<int>::max()||ny>std::numeric_limits<int>::max()||nx!=std::floor(nx)||ny!=std::floor(ny)||!finiteMatrix(xf.inv)) return;
    const int binsX=static_cast<int>(nx),binsY=static_cast<int>(ny);
    if(static_cast<size_t>(std::max(binsX,binsY))>std::numeric_limits<size_t>::max()/sizeof(float)/static_cast<size_t>(K)) return;
    for(int k=0;k<K;++k) {
        float peak[2]={-std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity()}; int index[2]={}; bool valid=true;
        for(int axis=0;axis<2;++axis) {
            const int bins=axis?binsY:binsX; const float* row=(axis?sy:sx)+static_cast<size_t>(k)*bins;
            for(int i=0;i<bins;++i) {
                if(!std::isfinite(row[i])) { valid=false; break; }
                if(row[i]>peak[axis]) { peak[axis]=row[i]; index[axis]=i; }
            }
        }
        if(!valid) continue;
        const double x=index[0]/static_cast<double>(split),y=index[1]/static_cast<double>(split);
        const double ix=(xf.inv[0]*x+xf.inv[1]*y+xf.inv[2])/imgW,iy=(xf.inv[3]*x+xf.inv[4]*y+xf.inv[5])/imgH;
        if(!std::isfinite(ix)||!std::isfinite(iy)) continue;
        out[k]={static_cast<float>(std::clamp(ix,0.,1.)),static_cast<float>(std::clamp(iy,0.,1.)),std::clamp(std::min(peak[0],peak[1]),0.f,1.f)};
        if(ix<0||ix>1||iy<0||iy>1) out[k].conf=0;
    }
}
Letterbox letterboxRtmDetToChw(const CameraFrame& f,int inW,int inH,std::vector<float>& out) {
    const auto lb=letterboxRgbToChw(f,inW,inH,out);
    constexpr float mean[]={103.53f,116.28f,123.675f},stddev[]={57.375f,57.12f,58.395f};
    const size_t channel=out.size()/3;
    for(size_t c=0;c<3;++c) for(size_t i=0;i<channel;++i) out[c*channel+i]=(out[c*channel+i]-mean[c])/stddev[c];
    return lb;
}
int decodeDetector(const float* dets,int n,const Letterbox& lb,int imgW,int imgH,float thr,DetBox* out,int maxOut) {
    if(!dets||!out||n<=0||maxOut<=0||imgW<=0||imgH<=0||!std::isfinite(thr)||!std::isfinite(lb.scale)||lb.scale<=0||!std::isfinite(lb.padX)||!std::isfinite(lb.padY)||static_cast<size_t>(n)>std::numeric_limits<size_t>::max()/sizeof(float)/5) return 0;
    int count=0,previousIndex=-1; float previous=std::numeric_limits<float>::infinity();
    // Supported end-to-end exports already perform NMS; retain their boxes in score order.
    for(int pass=0;pass<n&&count<maxOut;++pass) {
        int best=-1; float score=-std::numeric_limits<float>::infinity();
        for(int i=0;i<n;++i) {
            const float* d=dets+static_cast<size_t>(i)*5;
            if(!std::isfinite(d[0])||!std::isfinite(d[1])||!std::isfinite(d[2])||!std::isfinite(d[3])||!std::isfinite(d[4])||d[2]<=d[0]||d[3]<=d[1]||d[4]<thr||d[4]>previous||(d[4]==previous&&i<=previousIndex)) continue;
            if(d[4]>score) { best=i; score=d[4]; }
        }
        if(best<0) break;
        previous=score; previousIndex=best; const float* d=dets+static_cast<size_t>(best)*5; DetBox box; box.score=score;
        for(int j=0;j<4;++j) box.box[j]=static_cast<float>(std::clamp((static_cast<double>(d[j])-(j%2?lb.padY:lb.padX))/lb.scale,0.,static_cast<double>(j%2?imgH:imgW)));
        if(box.box[2]>box.box[0]&&box.box[3]>box.box[1]) out[count++]=box;
    }
    return count;
}
bool filterDetectorLabels(std::vector<float>& dets,const std::vector<int64_t>& labels,const std::vector<int64_t>& ds,const std::vector<int64_t>& ls,std::string& err) {
    err.clear();
    if(ds.size()!=3||ls.size()!=2||ds[0]!=1||ls[0]!=1||ds[1]<0||ds[1]>std::numeric_limits<int>::max()||ds[2]!=5||ls[1]!=ds[1]||static_cast<uint64_t>(ds[1])>std::numeric_limits<size_t>::max()/sizeof(float)/5||dets.size()!=static_cast<size_t>(ds[1])*5||labels.size()!=static_cast<size_t>(ds[1])) { err="RTMDet requires dets float32 [1,N,5] and labels int64 [1,N]"; return false; }
    for(size_t i=0;i<labels.size();++i) if(labels[i]!=0) dets[i*5+4]=-std::numeric_limits<float>::infinity();
    return true;
}
bool TopDownPose::init(OrtRuntime& detector,OrtRuntime& keypoints,int numKeypoints,std::string& err) {
    detector_=nullptr; keypoints_=nullptr; err.clear();
    try {
        if(numKeypoints<=0||numKeypoints>kMaxKeypoints) throw std::runtime_error("Invalid top-down keypoint count");
        if(!inputShape(detector.inputs(),detShape_)||!inputShape(keypoints.inputs(),kpShape_)) throw std::runtime_error("Top-down models require static float32 [1,3,H,W] inputs");
        const auto det=detector.outputs(),kp=keypoints.outputs(); bool foundDet=false,foundLabels=false,foundX=false,foundY=false;
        for(size_t i=0;i<det.size();++i) {
            if(det[i].name=="dets"&&det[i].type==OrtTensorType::Float32) { detIndex_=i; foundDet=true; }
            if(det[i].name=="labels"&&det[i].type==OrtTensorType::Int64) { labelIndex_=i; foundLabels=true; }
        }
        for(size_t i=0;i<kp.size();++i) {
            if(kp[i].type!=OrtTensorType::Float32) throw std::runtime_error("SimCC outputs must be float32");
            if(kp[i].name=="simcc_x") { xIndex_=i; foundX=true; }
            if(kp[i].name=="simcc_y") { yIndex_=i; foundY=true; }
        }
        if(!foundDet||!foundLabels) throw std::runtime_error("RTMDet requires named dets float32 and labels int64 outputs");
        if(!foundX||!foundY) throw std::runtime_error("Keypoint model requires named simcc_x and simcc_y outputs");
        const auto& ds=det[detIndex_].shape; const auto& ls=det[labelIndex_].shape;
        if(ds.size()!=3||ls.size()!=2||ds[0]!=1||ls[0]!=1||ds[2]!=5||(ds[1]>=0&&ls[1]>=0&&ds[1]!=ls[1])) throw std::runtime_error("Invalid RTMDet output shapes");
        if(!simccShape(kp[xIndex_].shape,numKeypoints,static_cast<int>(kpShape_[3])*2)||!simccShape(kp[yIndex_].shape,numKeypoints,static_cast<int>(kpShape_[2])*2)) throw std::runtime_error("SimCC requires [1,K,W*2] and [1,K,H*2] outputs");
        detInput_.reserve(static_cast<size_t>(detShape_[2])*static_cast<size_t>(detShape_[3])*3);
        kpInput_.reserve(static_cast<size_t>(kpShape_[2])*static_cast<size_t>(kpShape_[3])*3);
        nkp_=numKeypoints; detector_=&detector; keypoints_=&keypoints; return true;
    } catch(const std::exception& e) { err=e.what(); return false; }
}
int TopDownPose::infer(const CameraFrame& f,float detThr,float kpThr,int maxObjects,PoseObject* out,std::string& err) {
    err.clear();
    if(!detector_||!keypoints_) { err="Top-down model is not initialized"; return 0; }
    if(!out||maxObjects<=0||!std::isfinite(detThr)||!std::isfinite(kpThr)) { err="Invalid top-down output buffer or thresholds"; return 0; }
    try {
        const auto lb=letterboxRtmDetToChw(f,static_cast<int>(detShape_[3]),static_cast<int>(detShape_[2]),detInput_);
        if(lb.scale<=0) throw std::runtime_error("Invalid top-down RGB frame");
        if(!detector_->run(detInput_.data(),detShape_,detOutputs_,labels_,detShapes_,err)) return 0;
        if(detIndex_>=detOutputs_.size()||labelIndex_>=labels_.size()||detIndex_>=detShapes_.size()||labelIndex_>=detShapes_.size()) throw std::runtime_error("Missing RTMDet outputs");
        if(!filterDetectorLabels(detOutputs_[detIndex_],labels_[labelIndex_],detShapes_[detIndex_],detShapes_[labelIndex_],err)) return 0;
        const int n=static_cast<int>(detShapes_[detIndex_][1]); boxes_.resize(static_cast<size_t>(std::min(n,maxObjects)));
        const int count=decodeDetector(detOutputs_[detIndex_].data(),n,lb,f.width,f.height,detThr,boxes_.data(),static_cast<int>(boxes_.size()));
        const int w=static_cast<int>(kpShape_[3]),h=static_cast<int>(kpShape_[2]);
        for(int i=0;i<count;++i) {
            const auto& box=boxes_[static_cast<size_t>(i)]; CropXf xf;
            if(!makeCrop(box.box,f.width,f.height,w,h,1.25f,xf)) throw std::runtime_error("Invalid top-down crop");
            warpCropToChw(f,xf,kpInput_);
            if(kpInput_.empty()) throw std::runtime_error("Invalid top-down crop pixels");
            if(!keypoints_->run(kpInput_.data(),kpShape_,kpOutputs_,kpShapes_,err)) return 0;
            if(xIndex_>=kpOutputs_.size()||yIndex_>=kpOutputs_.size()||xIndex_>=kpShapes_.size()||yIndex_>=kpShapes_.size()||!simccShape(kpShapes_[xIndex_],nkp_,w*2)||!simccShape(kpShapes_[yIndex_],nkp_,h*2)||kpOutputs_[xIndex_].size()!=static_cast<size_t>(nkp_)*static_cast<size_t>(w)*2||kpOutputs_[yIndex_].size()!=static_cast<size_t>(nkp_)*static_cast<size_t>(h)*2) throw std::runtime_error("Invalid runtime SimCC shapes or lengths");
            PoseObject pose; pose.nkp=nkp_; pose.score=box.score;
            for(int j=0;j<4;++j) pose.box[j]=box.box[j]/static_cast<float>(j%2?f.height:f.width);
            decodeSimcc(kpOutputs_[xIndex_].data(),kpOutputs_[yIndex_].data(),nkp_,w,h,2,xf,f.width,f.height,pose.kp);
            for(int j=0;j<nkp_;++j) if(pose.kp[j].conf<kpThr) pose.kp[j]={};
            out[i]=pose;
        }
        return count;
    } catch(const std::exception& e) { err=e.what(); return 0; }
}
}
