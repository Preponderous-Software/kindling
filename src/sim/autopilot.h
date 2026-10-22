// An autopilot that plays the MVP from the ruin to Ada sitting by the fire,
// through the same Input a player gives. It aims straight at targets (it has
// no engine to pick with), walks between them by way of the door, and works
// each one until the World says it is done.
//
// It is the end-to-end test of the rules, and the source of the reveal and the
// screenshots in CI: a real restoration, recorded as it happens.
#pragma once

#include <cstdint>
#include <set>
#include <string>
#include <utility>

#include "sim/rules.h"

namespace kindling {

class Autopilot {
public:
    // The next tick of input for the world as it is now.
    Input next(const World& w);

    // What it is doing, for logs ("sweeping the floor").
    const std::string& activity() const { return activity_; }

private:
    struct Goal {
        bool walking = false;
        float x = 0, z = 0;   // where to stand
        Aim aim;              // what to face and work on once there
        bool use = false;     // hold the primary action
        bool press = false;   // press interact once on arrival
        Tool tool = Tool::None;
    };

    Goal choose(const World& w);
    Input steer(const World& w, const Goal& g);

    std::string activity_;
    int32_t stuckTicks_ = 0;
    float lastX_ = 0, lastZ_ = 0;
    int64_t targetKey_ = -1;      // the grime texel or job being worked, for the stuck check
    int32_t targetTicks_ = 0;
    std::set<int64_t> skipped_;   // grime texels it gave up on
    bool pressedLast_ = false;
};

}  // namespace kindling
