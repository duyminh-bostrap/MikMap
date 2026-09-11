#pragma once

#include "Screen.h"
#include <string>
#include <vector>

namespace HexMap::Core::Model {

struct Clip {
    std::string id;
    std::string name;
    std::string filepath;
    bool active = false;
    double duration = 10.0;
    double position = 0.0;
};

struct Layer {
    std::string id;
    std::string name;
    float opacity = 1.0f;
    int blendMode = 0;
    std::vector<Clip> clips;
};

class Composition {
public:
    std::string name = "MikMap Project";
    int canvasWidth = 1920;
    int canvasHeight = 1080;
    float masterOpacity = 1.0f;
    float bpm = 120.0f;

    std::vector<Layer> layers;
    std::vector<Screen> screens;
};

} // namespace HexMap::Core::Model
