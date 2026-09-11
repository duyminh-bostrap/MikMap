#pragma once

#include "Slice.h"
#include <string>
#include <vector>

namespace HexMap::Core::Model {

class Screen {
public:
    std::string id;
    std::string name;
    std::string outputDevice = "Projector 1";
    int width = 1920;
    int height = 1080;
    int fps = 60;
    bool edgeBlending = true;
    std::vector<Slice> slices;
};

} // namespace HexMap::Core::Model
