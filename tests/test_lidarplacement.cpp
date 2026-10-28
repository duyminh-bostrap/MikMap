#include "TestHarness.h"

#include "core/calib/LidarPlacement.h"

#include <random>

using namespace mikmap;

TEST_CASE("lidar placement: lidar o giua mep duoi tuong 4000x2250, quay mat len") {
    LidarPose p{2000, 2250, -90, false};
    REQUIRE_VEC_NEAR(lidarToSurface(p, {1000, 0}), 2000, 1250, 1e-9);
    REQUIRE_VEC_NEAR(lidarToSurface(p, {1000, 500}), 1500, 1250, 1e-9);
    p.mirror = true;
    REQUIRE_VEC_NEAR(lidarToSurface(p, {1000, 500}), 2500, 1250, 1e-9);
}

TEST_CASE("lidar placement: khu hoi surfaceToLidar nghich dao lidarToSurface") {
    std::mt19937 rng(12345);
    std::uniform_real_distribution<double> u(-5000, 5000), a(-180, 180);
    for (int i = 0; i < 3; ++i) {
        LidarPose p{u(rng), u(rng), a(rng), i == 1};
        const Vec2 v{u(rng), u(rng)};
        REQUIRE_VEC_NEAR(surfaceToLidar(p, lidarToSurface(p, v)), v.x, v.y, 1e-9);
    }
}
