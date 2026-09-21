#include "core/model/TriggerZone.h"

#include <cstring>

namespace hexmap {

const char* triggerActionName(TriggerAction a) {
    switch (a) {
    case TriggerAction::TriggerClip:   return "TriggerClip";
    case TriggerAction::TriggerColumn: return "TriggerColumn";
    case TriggerAction::ClearLayer:    return "ClearLayer";
    case TriggerAction::ClearAll:      return "ClearAll";
    default:                           return "None";
    }
}

TriggerAction triggerActionFromName(const char* name) {
    if (name == nullptr) return TriggerAction::None;
    if (std::strcmp(name, "TriggerClip")   == 0) return TriggerAction::TriggerClip;
    if (std::strcmp(name, "TriggerColumn") == 0) return TriggerAction::TriggerColumn;
    if (std::strcmp(name, "ClearLayer")    == 0) return TriggerAction::ClearLayer;
    if (std::strcmp(name, "ClearAll")      == 0) return TriggerAction::ClearAll;
    return TriggerAction::None;
}

std::vector<TriggerHit> TriggerZoneSet::update(const std::vector<Vec2>& canvasPoints,
                                               double nowSec) {
    std::vector<TriggerHit> hits;

    // ── Bước 1: xác định vùng nào đang có điểm ─────────────────────────
    for (TriggerZone& z : zones) {
        z.wasOccupied = z.occupied;
        z.occupied = false;
    }

    // Với MỖI điểm, chỉ vùng TRÊN CÙNG chứa nó được tính. Nếu không,
    // một điểm rơi vào vùng nhỏ nằm trên cũng đánh thức luôn vùng nền
    // phía dưới — người vận hành thấy hai clip cùng nhảy ra.
    for (const Vec2& p : canvasPoints) {
        for (int i = static_cast<int>(zones.size()) - 1; i >= 0; --i) {
            TriggerZone& z = zones[static_cast<size_t>(i)];
            if (!z.enabled) continue;
            if (!z.contains(p)) continue;
            z.occupied = true;
            break;                    // vùng trên cùng thắng
        }
    }

    // ── Bước 2: phát hiện cạnh lên ─────────────────────────────────────
    for (int i = 0; i < static_cast<int>(zones.size()); ++i) {
        TriggerZone& z = zones[static_cast<size_t>(i)];

        if (!z.enabled) continue;
        if (!z.occupied || z.wasOccupied) continue;   // không phải cạnh lên
        if (!z.isReady(nowSec)) continue;             // đang chống dội

        z.lastFiredSec = nowSec;

        TriggerHit h;
        h.fired     = true;
        h.zoneIndex = i;
        h.action    = z.action;
        h.layer     = z.targetLayer;
        h.column    = z.targetColumn;
        hits.push_back(h);
    }

    return hits;
}

void TriggerZoneSet::resetRuntimeState() {
    for (TriggerZone& z : zones) {
        z.lastFiredSec = -1.0e9;
        z.occupied = false;
        z.wasOccupied = false;
    }
}

} // namespace hexmap
