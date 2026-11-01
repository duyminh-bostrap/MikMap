#include "TestHarness.h"
#include "io/ai/PersonTracker.h"
#include <limits>
#include <type_traits>
using namespace mikmap;
namespace {
PersonPose person(float x,float score=1) {
    PersonPose p; p.score=score;
    for(int k=0;k<17;++k) p.kp[k]={x+(k%2 ? .12f : -.12f), .2f+float(k)*.03f,1};
    return p;
}
}
TEST_CASE("Person tracker preserves crossing trajectories and raw coordinates") {
    PersonTracker tracker; TrackedPerson out[30];
    for(int f=0;f<10;++f) {
        PersonPose p[]={person(.2f+float(f)*.065f),person(.8f-float(f)*.065f)};
        // Reverse detection ordering every frame, including after crossing.
        if(f%2) std::swap(p[0],p[1]);
        REQUIRE(tracker.update(p,2,double(f)*.05,out,30)==2);
        CHECK(out[0].slot==0); CHECK(out[1].slot==1);
        CHECK(out[0].pose.kp[0].x==person(.2f+float(f)*.065f).kp[0].x);
        CHECK(out[1].pose.kp[0].x==person(.8f-float(f)*.065f).kp[0].x);
    }
}
TEST_CASE("Person tracker retains lost slots then reuses lowest expired slot") {
    PersonTracker t; TrackedPerson out[30]; PersonPose p[]={person(.2f),person(.8f)};
    REQUIRE(t.update(p,2,0,out,30)==2);
    CHECK(t.update(p+1,1,.2,out,30)==2);
    REQUIRE(t.update(p,2,.4,out,30)==2); CHECK(out[0].lastSeen==.4);
    CHECK(t.update(p+1,1,.8,out,30)==2);
    REQUIRE(t.update(p+1,1,1.0,out,30)==1); CHECK(out[0].slot==1);
    REQUIRE(t.update(p,2,1.1,out,30)==2); CHECK(out[0].slot==0);
}
TEST_CASE("Person tracker confidence cap runtime reduction and bounds") {
    PersonTracker t; TrackedPerson out[30]; PersonPose p[35];
    for(int i=0;i<35;++i) p[i]=person(float(i),float(i)/35);
    REQUIRE(t.update(p,15,0,out,30)==10);
    for(int i=0;i<10;++i) { CHECK(out[i].slot==i); CHECK(out[i].pose.score>=p[5].score); }
    TrackerParams params; params.maxPeople=3; t.setParams(params);
    REQUIRE(t.update(nullptr,0,.1,out,30)==3);
    for(int i=0;i<3;++i) CHECK(out[i].slot==i);
    params.maxPeople=100; t.setParams(params); t.reset();
    CHECK(t.update(p,35,0,out,30)==30);
    params.maxPeople=0; t.setParams(params); CHECK(t.update(nullptr,0,.1,out,30)==1);
}
TEST_CASE("Person tracker invalid inputs time and smoothing") {
    PersonTracker t; TrackedPerson out[30]; PersonPose p=person(.2f);
    REQUIRE(t.update(&p,1,1,out,30)==1);
    CHECK(t.update(nullptr,4,1.1,out,30)==1);
    CHECK(t.update(&p,1,std::numeric_limits<double>::quiet_NaN(),out,30)==0);
    REQUIRE(t.update(&p,1,.5,out,30)==1); CHECK(out[0].lastSeen>=1);
    CHECK(t.update(&p,1,1.2,nullptr,30)==0);
    CHECK(t.update(&p,1,1.2,out,0)==0);
    TrackerParams params; params.smoothing=1; t.setParams(params);
    p=person(.25f); REQUIRE(t.update(&p,1,1.3,out,30)==1);
    CHECK(out[0].pose.kp[0].x>person(.2f).kp[0].x); CHECK(out[0].pose.kp[0].x<p.kp[0].x);
    params.keepSec=-1; params.matchIou=std::numeric_limits<float>::quiet_NaN(); t.setParams(params);
    p.score=std::numeric_limits<float>::quiet_NaN(); CHECK(t.update(&p,1,2,out,30)==0);
}

TEST_CASE("Person tracker tiny time delta keeps finite motion prediction") {
    PersonTracker t; TrackedPerson out[30]; PersonPose p=person(.2f);
    REQUIRE(t.update(&p,1,0,out,30)==1);
    p=person(.21f); REQUIRE(t.update(&p,1,1e-300,out,30)==1);
    p=person(.22f); REQUIRE(t.update(&p,1,.1,out,30)==1);
    CHECK(out[0].slot==0); CHECK(out[0].pose.kp[0].x==p.kp[0].x);
}

TEST_CASE("Generic poses bound storage and preserve the legacy default") {
    static_assert(sizeof(PoseObject)<=2048);
    static_assert(std::is_trivially_copyable<PoseObject>::value);
    static_assert(std::is_same<PersonPose,PoseObject>::value);
    static_assert(kMaxKeypoints==133); CHECK(PoseObject{}.nkp==17);
}
TEST_CASE("Hand tracker preserves crossing trajectories and all keypoints") {
    PersonTracker tracker; TrackedPerson out[30];
    for(int f=0;f<10;++f) {
        PoseObject p[2];
        for(int i=0;i<2;++i) {
            p[i].nkp=21; p[i].side=static_cast<int8_t>(i); p[i].score=1;
            const float x=i==0 ? .2f+float(f)*.065f : .8f-float(f)*.065f;
            for(int k=0;k<21;++k) p[i].kp[k]={x+(k%2 ? .12f : -.12f),.2f+float(k)*.03f,1};
        }
        const float x=p[0].kp[20].x;
        if(f%2) std::swap(p[0],p[1]);
        REQUIRE(tracker.update(p,2,double(f)*.05,out,30)==2);
        CHECK(out[0].pose.kp[20].x==x); CHECK(out[0].pose.side==0); CHECK(out[1].pose.side==1);
    }
}
TEST_CASE("Generic tracker rejects invalid counts and separates layouts") {
    PersonTracker t; TrackedPerson out[30]; PoseObject p=person(.4f);
    REQUIRE(t.update(&p,1,0,out,30)==1);
    p.nkp=21; REQUIRE(t.update(&p,1,.1,out,30)==2);
    CHECK(out[0].pose.nkp==17); CHECK(out[0].lastSeen==0); CHECK(out[1].pose.nkp==21);
    for(int n:{0,-1,134}) { t.reset(); p.nkp=n; CHECK(t.update(&p,1,0,out,30)==0); }
}
TEST_CASE("Hand side majority ignores unknown votes and resets on new tracks") {
    PersonTracker t; TrackedPerson out[30]; PoseObject p=person(.4f); p.nkp=21;
    const int sides[]={0,1,-1,1,0,-1,-1,-1,-1,-1};
    const int expected[]={0,0,0,1,1,1,1,1,0,0};
    for(int f=0;f<10;++f) {
        p.side=static_cast<int8_t>(sides[f]); REQUIRE(t.update(&p,1,double(f)*.05,out,30)==1);
        CHECK(out[0].pose.side==expected[f]);
    }
    p.side=-1; REQUIRE(t.update(&p,1,2,out,30)==1); CHECK(out[0].pose.side==-1);
}
TEST_CASE("Generic tracker smooths actual points and ignores points outside layout") {
    PersonTracker t; TrackerParams params; params.smoothing=1; t.setParams(params);
    TrackedPerson out[30]; PoseObject p=person(.4f); p.nkp=133; p.kp[132]={.4f,.5f,1};
    REQUIRE(t.update(&p,1,0,out,30)==1); p.kp[132].x=.45f;
    REQUIRE(t.update(&p,1,.1,out,30)==1); CHECK_NEAR(out[0].pose.kp[132].x,.4325f,1e-6);
    t.reset(); p={}; p.nkp=21; p.score=1; p.kp[100]={.5f,.5f,1};
    CHECK(t.update(&p,1,0,out,30)==0);
}

TEST_CASE("Generic tracker holds thirty whole-body tracks in fixed storage") {
    static_assert(std::is_trivially_copyable<PersonTracker>::value);
    static_assert(std::is_standard_layout<PoseObject>::value);
    PersonTracker t; TrackerParams params; params.maxPeople=30; t.setParams(params);
    PoseObject poses[30]; TrackedPerson out[30];
    for(int i=0;i<30;++i) {
        poses[i]=person(float(i)); poses[i].nkp=133;
        poses[i].kp[132]={float(i),.5f,1};
    }
    for(int f=0;f<3;++f) {
        REQUIRE(t.update(poses,30,double(f)*.05,out,30)==30);
        for(int i=0;i<30;++i) { CHECK(out[i].slot==i); CHECK(out[i].pose.kp[132].x==float(i)); }
    }
}
TEST_CASE("RTMO explicitly emits the Body layout into a reused generic object") {
    float dets[]={0,0,640,640,1}; float kp[51]{}; PoseObject p; p.nkp=133; p.side=1;
    REQUIRE(decodeRtmo(dets,kp,1,Letterbox{},640,640,.5f,.5f,&p,1)==1);
    CHECK(p.nkp==17); CHECK(p.side==-1); CHECK(p.kp[132].conf==0);
}
