#pragma once
#include "PoseModel.h"
#include <array>
namespace mikmap {
struct TrackerParams { int maxPeople=10; double keepSec=.5; float matchIou=.3f; int smoothing=0; };
struct TrackedPerson { int slot=0; PersonPose pose; double lastSeen=0; };
class PersonTracker {
public:
    void setParams(const TrackerParams&);
    // Returns written tracks (including retained unseen poses), ordered by slot; invalid time/output is a no-op.
    // Backward timestamps clamp to the last accepted time. Parameters clamp to supported ranges.
    int update(const PersonPose* people,int n,double tSec,TrackedPerson* out,int maxOut);
    void reset();
private:
    struct Track { bool active=false; TrackedPerson person; PersonPose raw; float vx=0,vy=0; };
    TrackerParams params_;
    std::array<Track,30> tracks_{};
    double time_=0;
};
}
