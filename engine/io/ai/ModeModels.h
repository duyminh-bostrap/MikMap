#pragma once
#include "PoseModel.h"
#include <limits>

namespace mikmap {
struct ModeSpec {
    CamMode mode;
    const char* pack;
    const char* detectorFile;
    const char* keypointFile;
    int keypointsPerObject;
    int objectsCap;
};
// Body's canonical filename is descriptive; loading keeps the existing rtmo-s/m manifest selection.
const ModeSpec& modeSpec(CamMode mode);
int clampObjects(CamMode mode, int requested);
// Invalid mode, slot, keypoint or unknown Hand side sets emit=false; outputs are otherwise unchanged.
void pointGroupAndId(CamMode mode, int slot, int8_t side, int k, uint8_t& group, uint32_t& id, bool& emit);
inline constexpr uint32_t kCalibUnavailable=std::numeric_limits<uint32_t>::max();
uint32_t calibPointId(CamMode mode, int faceNoseIndex=-1);
// Palm toward camera, image y down. Invisible, invalid or collinear landmarks return -1 (unknown).
int8_t handSide(const PoseObject& hand, bool flipped);
}
