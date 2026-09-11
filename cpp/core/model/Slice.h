#pragma once

#include "IWarp.h"
#include "WarpCornerPin.h"
#include <string>
#include <vector>
#include <memory>

namespace HexMap::Core::Model {

struct Rect {
    double x = 0.0;
    double y = 0.0;
    double w = 1920.0;
    double h = 1080.0;
};

class Slice {
public:
    std::string id;
    std::string name;
    bool visible = true;
    Rect inputRect;
    std::unique_ptr<IWarp> warp;

    Slice() {
        warp = std::make_unique<WarpCornerPin>();
    }

    Slice(const Slice& o)
        : id(o.id), name(o.name), visible(o.visible), inputRect(o.inputRect) {
        if (o.warp) warp = o.warp->clone();
    }

    Slice& operator=(const Slice& o) {
        if (this != &o) {
            id = o.id;
            name = o.name;
            visible = o.visible;
            inputRect = o.inputRect;
            if (o.warp) warp = o.warp->clone();
        }
        return *this;
    }
};

} // namespace HexMap::Core::Model
