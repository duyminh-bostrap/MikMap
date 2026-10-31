#include "TestHarness.h"
#include "io/ai/PersonTracker.h"
#include <limits>
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

