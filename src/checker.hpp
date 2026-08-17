#pragma once

#include <cstdint>
#include <string>
#include <vector>

class GJGameLevel;

struct VerificationResult {
    bool completed = false;
    bool died = false;
    int frame = 0;
    int objectID = 0;
    float objectX = 0;
    float objectY = 0;
    std::string error;
};

// Replays a candidate through a real PlayLayer. This must be called on the
// cocos thread; it intentionally uses Geometry Dash's object list, trigger
// graph, movement and collision callbacks rather than gd-sim.
VerificationResult verifyInGame(GJGameLevel* level, std::vector<uint8_t> const& macro);
