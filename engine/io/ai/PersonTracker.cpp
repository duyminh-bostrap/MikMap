#include "PersonTracker.h"
#include <algorithm>
#include <cmath>
#include <limits>
namespace mikmap {
namespace {
struct Bounds { float x0=0,y0=0,x1=0,y1=0,hx=0,hy=0; bool valid=false; };
Bounds bounds(const PersonPose& p) {
    Bounds b;
    for(int i=0;i<p.nkp;++i) if(const auto& k=p.kp[i]; k.conf>0 && std::isfinite(k.x) && std::isfinite(k.y)) {
        if(!b.valid) { b.x0=b.x1=k.x; b.y0=b.y1=k.y; b.valid=true; }
        else { b.x0=std::min(b.x0,k.x); b.x1=std::max(b.x1,k.x); b.y0=std::min(b.y0,k.y); b.y1=std::max(b.y1,k.y); }
    }
    b.hx=(b.x0+b.x1)*.5f; b.hy=(b.y0+b.y1)*.5f;
    if((p.nkp==17 || p.nkp==133) && p.kp[11].conf>0 && p.kp[12].conf>0 && std::isfinite(p.kp[11].x) && std::isfinite(p.kp[12].x) && std::isfinite(p.kp[11].y) && std::isfinite(p.kp[12].y)) {
        b.hx=(p.kp[11].x+p.kp[12].x)*.5f; b.hy=(p.kp[11].y+p.kp[12].y)*.5f;
    }
    return b;
}
float iou(Bounds a,Bounds b) {
    const float overlap=std::max(0.f,std::min(a.x1,b.x1)-std::max(a.x0,b.x0))*std::max(0.f,std::min(a.y1,b.y1)-std::max(a.y0,b.y0));
    const float area=(a.x1-a.x0)*(a.y1-a.y0)+(b.x1-b.x0)*(b.y1-b.y0)-overlap;
    return area>0 ? overlap/area : 0;
}
}
void PersonTracker::Track::updateSide(int8_t side) {
    if(person.pose.nkp!=21) return;
    sides[sideNext]=side==0 || side==1 ? side : int8_t(-1);
    sideNext=(sideNext+1)%5;
    int left=0,right=0;
    for(auto vote:sides) { left+=vote==0; right+=vote==1; }
    // Unknown frames age out votes; ties (including no votes) retain the assigned side.
    if(left!=right) stableSide=left>right ? int8_t(0) : int8_t(1);
    person.pose.side=stableSide;
}
void PersonTracker::setParams(const TrackerParams& p) {
    params_=p; params_.maxPeople=std::clamp(p.maxPeople,1,30);
    params_.keepSec=std::isfinite(p.keepSec) ? std::max(0.,p.keepSec) : .5;
    params_.matchIou=std::isfinite(p.matchIou) ? std::clamp(p.matchIou,0.f,1.f) : .3f;
    params_.smoothing=std::clamp(p.smoothing,0,2);
    for(int i=params_.maxPeople;i<30;++i) tracks_[i]={};
}
void PersonTracker::reset() { tracks_={}; time_=0; }
int PersonTracker::update(const PersonPose* people,int n,double tSec,TrackedPerson* out,int maxOut) {
    if(!std::isfinite(tSec) || !out || maxOut<=0) return 0;
    const double now=std::max(time_,tSec); time_=now;
    std::array<int,30> selected{}; int count=0;
    for(int i=0;people && i<n;++i) {
        if(people[i].nkp<=0 || people[i].nkp>kMaxKeypoints || !std::isfinite(people[i].score) || !bounds(people[i]).valid) continue;
        int at=0; while(at<count && people[selected[at]].score>=people[i].score) ++at;
        if(at>=params_.maxPeople) continue;
        count=std::min(count+1,params_.maxPeople);
        for(int j=count-1;j>at;--j) selected[j]=selected[j-1];
        selected[at]=i;
    }
    std::array<bool,30> used{},matched{};
    for(auto& track:tracks_) if(track.active && now-track.person.lastSeen>params_.keepSec) track={};
    for(int pass=0;pass<count;++pass) {
        int bestSlot=-1,bestDetection=-1; float bestIou=-1,bestDistance=std::numeric_limits<float>::max();
        for(int s=0;s<params_.maxPeople;++s) if(tracks_[s].active && !matched[s]) {
            const auto& track=tracks_[s]; auto predicted=bounds(track.raw);
            const float dt=static_cast<float>(now-track.person.lastSeen);
            const float dx=track.vx*dt,dy=track.vy*dt;
            predicted.x0+=dx; predicted.x1+=dx; predicted.hx+=dx; predicted.y0+=dy; predicted.y1+=dy; predicted.hy+=dy;
            if(!std::isfinite(predicted.x0) || !std::isfinite(predicted.x1) || !std::isfinite(predicted.y0) || !std::isfinite(predicted.y1) || !std::isfinite(predicted.hx) || !std::isfinite(predicted.hy)) predicted=bounds(track.raw);
            for(int d=0;d<count;++d) if(!used[d] && people[selected[d]].nkp==track.raw.nkp) {
                const auto b=bounds(people[selected[d]]); const float overlap=iou(predicted,b);
                const float distance=std::hypot(predicted.hx-b.hx,predicted.hy-b.hy);
                if(overlap<params_.matchIou) continue;
                if(overlap>bestIou || (overlap==bestIou && distance<bestDistance)) { bestIou=overlap; bestDistance=distance; bestSlot=s; bestDetection=d; }
            }
        }
        if(bestSlot<0) break;
        auto& track=tracks_[bestSlot]; const auto& raw=people[selected[bestDetection]];
        const double dt=now-track.person.lastSeen;
        if(dt>0) {
            const auto a=bounds(track.raw),b=bounds(raw);
            const double vx=(double(b.hx)-a.hx)/dt,vy=(double(b.hy)-a.hy)/dt;
            const double limit=std::numeric_limits<float>::max();
            const bool finite=std::isfinite(vx) && std::isfinite(vy) && std::abs(vx)<=limit && std::abs(vy)<=limit;
            track.vx=finite ? static_cast<float>(vx) : 0; track.vy=finite ? static_cast<float>(vy) : 0;
        }
        auto pose=raw;
        if(params_.smoothing) for(int k=0;k<raw.nkp;++k) if(raw.kp[k].conf>0 && track.person.pose.kp[k].conf>0) {
            const float alpha=params_.smoothing==1 ? .65f : .35f;
            pose.kp[k].x=track.person.pose.kp[k].x*(1-alpha)+raw.kp[k].x*alpha;
            pose.kp[k].y=track.person.pose.kp[k].y*(1-alpha)+raw.kp[k].y*alpha;
        }
        track.raw=raw; track.person={bestSlot,pose,now}; track.updateSide(raw.side); matched[bestSlot]=true; used[bestDetection]=true;
    }
    for(int d=0;d<count;++d) if(!used[d]) {
        int slot=0; while(slot<params_.maxPeople && tracks_[slot].active) ++slot;
        if(slot==params_.maxPeople) {
            for(int s=0;s<params_.maxPeople;++s) if(!matched[s] && (slot==params_.maxPeople || tracks_[s].person.pose.score<tracks_[slot].person.pose.score)) slot=s;
            if(slot==params_.maxPeople || tracks_[slot].person.pose.score>=people[selected[d]].score) continue;
        }
        tracks_[slot]={true,{slot,people[selected[d]],now},people[selected[d]],0,0}; tracks_[slot].updateSide(people[selected[d]].side); matched[slot]=true;
    }
    int written=0;
    for(int s=0;s<params_.maxPeople && written<maxOut;++s) if(tracks_[s].active) out[written++]=tracks_[s].person;
    return written;
}
}
