#include "PoseModel.h"
#include <algorithm>
#include <cmath>
#include <limits>
namespace mikmap {
const int kCocoToMediaPipe[17]={0,2,5,7,8,11,12,13,14,15,16,23,24,25,26,27,28};
namespace {
bool pixels(int w,int h,size_t& count) {
    if(w<=0 || h<=0 || static_cast<size_t>(w)>std::numeric_limits<size_t>::max()/3/static_cast<size_t>(h)) return false;
    count=static_cast<size_t>(w)*static_cast<size_t>(h); return true;
}
bool finite(float x) { return std::isfinite(x); }
float coord(float x,float pad,float scale,int dim) { return std::clamp((x-pad)/scale/static_cast<float>(dim),0.f,1.f); }
double iou(const float* a,const float* b) {
    const double intersection=std::max(0.0,static_cast<double>(std::min(a[2],b[2]))-std::max(a[0],b[0]))*std::max(0.0,static_cast<double>(std::min(a[3],b[3]))-std::max(a[1],b[1]));
    const double total=(static_cast<double>(a[2])-a[0])*(static_cast<double>(a[3])-a[1])+(static_cast<double>(b[2])-b[0])*(static_cast<double>(b[3])-b[1])-intersection;
    return total>0?intersection/total:0;
}
}
Letterbox letterboxRgbToChw(const CameraFrame& f,int inW,int inH,std::vector<float>& out) {
    Letterbox lb{0,0,0,inW,inH}; size_t src=0,dst=0;
    if(!pixels(f.width,f.height,src)||!pixels(inW,inH,dst)||f.rgb.size()!=src*3||dst>out.max_size()/3) { out.clear(); return lb; }
    lb.scale=std::min(static_cast<float>(inW)/f.width,static_cast<float>(inH)/f.height);
    const int w=std::min(inW,static_cast<int>(f.width*lb.scale)),h=std::min(inH,static_cast<int>(f.height*lb.scale));
    if(w<=0||h<=0) { lb.scale=0; out.clear(); return lb; }
    out.resize(dst*3); std::fill(out.begin(),out.end(),114.f);
    for(int y=0;y<h;++y) {
        const float sy=std::clamp((static_cast<float>(y)+.5f)*f.height/h-.5f,0.f,static_cast<float>(f.height-1));
        const int y0=static_cast<int>(sy),y1=std::min(y0+1,f.height-1); const float fy=sy-y0;
        for(int x=0;x<w;++x) {
            const float sx=std::clamp((static_cast<float>(x)+.5f)*f.width/w-.5f,0.f,static_cast<float>(f.width-1));
            const int x0=static_cast<int>(sx),x1=std::min(x0+1,f.width-1); const float fx=sx-x0;
            for(int c=0;c<3;++c) {
                const auto at=[&](int px,int py) { return static_cast<float>(f.rgb[(static_cast<size_t>(py)*f.width+px)*3+2-c]); };
                const float top=at(x0,y0)*(1-fx)+at(x1,y0)*fx,bottom=at(x0,y1)*(1-fx)+at(x1,y1)*fx;
                out[static_cast<size_t>(c)*dst+static_cast<size_t>(y)*inW+x]=std::floor(top*(1-fy)+bottom*fy+.5f);
            }
        }
    }
    return lb;
}
int decodeRtmo(const float* dets,const float* kps,int n,const Letterbox& lb,int imgW,int imgH,float personThr,float jointThr,PersonPose* out,int maxOut) {
    if(n<=0||!dets||!kps||!out||maxOut<=0||imgW<=0||imgH<=0||!finite(lb.scale)||lb.scale<=0||!finite(lb.padX)||!finite(lb.padY)||!finite(personThr)||!finite(jointThr)||static_cast<size_t>(n)>std::numeric_limits<size_t>::max()/sizeof(float)/51) return 0;
    int count=0; std::vector<int> selected; selected.reserve(static_cast<size_t>(std::min(n,maxOut)));
    // Select in score order, retaining only the strongest non-overlapping people.
    float previous=std::numeric_limits<float>::infinity(); int previousIndex=-1;
    for(int pass=0;pass<n && count<maxOut;++pass) {
        int best=-1; float score=-std::numeric_limits<float>::infinity();
        for(int i=0;i<n;++i) {
            const float* d=dets+static_cast<size_t>(i)*5;
            if(!finite(d[0])||!finite(d[1])||!finite(d[2])||!finite(d[3])||!finite(d[4])||d[2]<d[0]||d[3]<d[1]||d[4]<personThr) continue;
            if(d[4]>previous||(d[4]==previous&&i<=previousIndex)) continue;
            if(d[4]>score) { best=i; score=d[4]; }
        }
        if(best<0) break;
        previous=score; previousIndex=best;
        const float* d=dets+static_cast<size_t>(best)*5; PersonPose p; p.score=score;
        for(int j=0;j<4;++j) p.box[j]=coord(d[j],j%2?lb.padY:lb.padX,lb.scale,j%2?imgH:imgW);
        bool suppressed=false; for(int j=0;j<count;++j) if(iou(d,dets+static_cast<size_t>(selected[static_cast<size_t>(j)])*5)>.45f) { suppressed=true; break; }
        if(suppressed) continue;
        const float* k=kps+static_cast<size_t>(best)*51;
        for(int j=0;j<17;++j) if(finite(k[j*3])&&finite(k[j*3+1])&&finite(k[j*3+2])&&k[j*3+2]>=jointThr) p.kp[j]={coord(k[j*3],lb.padX,lb.scale,imgW),coord(k[j*3+1],lb.padY,lb.scale,imgH),k[j*3+2]};
        out[count++]=p; selected.push_back(best);
    }
    return count;
}
bool PoseModel::init(OrtRuntime& rt,std::string& err) {
    rt_=nullptr; err.clear(); const auto inputs=rt.inputs(),outputs=rt.outputs();
    if(inputs.size()!=1||inputs[0].shape.size()!=4) { err="RTMO requires one static NCHW input"; return false; }
    const auto& s=inputs[0].shape;
    if(s[0]!=1||s[1]!=3||s[2]<=0||s[3]<=0||s[2]>std::numeric_limits<int>::max()||s[3]>std::numeric_limits<int>::max()) { err="RTMO requires static input [1,3,H,W]; dynamic input dimensions unsupported"; return false; }
    size_t count=0; if(!pixels(static_cast<int>(s[3]),static_cast<int>(s[2]),count)||count>input_.max_size()/3) { err="RTMO input size overflows"; return false; }
    bool det=false,kp=false;
    for(size_t i=0;i<outputs.size();++i) { if(outputs[i].name=="dets") { detIndex_=i; det=true; } if(outputs[i].name=="keypoints") { kpIndex_=i; kp=true; } }
    if(!det||!kp) { err="RTMO requires named dets and keypoints outputs"; return false; }
    shape_=s; input_.reserve(count*3); rt_=&rt; return true;
}
int PoseModel::infer(const CameraFrame& f,float personThr,float jointThr,PersonPose* out,int maxOut,std::string& err) {
    err.clear(); if(!rt_) { err="RTMO model is not initialized"; return 0; }
    if(!out||maxOut<=0||!finite(personThr)||!finite(jointThr)) { err="Invalid RTMO output buffer or thresholds"; return 0; }
    const auto lb=letterboxRgbToChw(f,static_cast<int>(shape_[3]),static_cast<int>(shape_[2]),input_);
    if(lb.scale<=0) { err="Invalid RTMO RGB frame"; return 0; }
    if(!rt_->run(input_.data(),shape_,outputs_,outputShapes_,err)) return 0;
    if(detIndex_>=outputs_.size()||kpIndex_>=outputs_.size()||detIndex_>=outputShapes_.size()||kpIndex_>=outputShapes_.size()) { err="Missing RTMO output tensors"; return 0; }
    const auto& d=outputShapes_[detIndex_]; const auto& k=outputShapes_[kpIndex_];
    if(d.size()!=3||k.size()!=4||d[0]!=1||k[0]!=1||d[1]<0||d[1]>std::numeric_limits<int>::max()||k[1]!=d[1]||d[2]!=5||k[2]!=17||k[3]!=3||static_cast<uint64_t>(d[1])>std::numeric_limits<size_t>::max()/sizeof(float)/51||outputs_[detIndex_].size()!=static_cast<size_t>(d[1])*5||outputs_[kpIndex_].size()!=static_cast<size_t>(d[1])*51) { err="Invalid RTMO dets [1,N,5] or keypoints [1,N,17,3]"; return 0; }
    return decodeRtmo(outputs_[detIndex_].data(),outputs_[kpIndex_].data(),static_cast<int>(d[1]),lb,f.width,f.height,personThr,jointThr,out,maxOut);
}
}
