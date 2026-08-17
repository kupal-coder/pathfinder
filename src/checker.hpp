#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
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
    float objectRotation = 0;
    float objectScaleX = 1;
    float objectScaleY = 1;
    bool player2 = false;
    float playerX = 0;
    float playerY = 0;
    double playerYVelocity = 0;
    uint64_t traceHash = 1469598103934665603ull;
    std::string error;
};

// Replays a candidate through a real PlayLayer. This must be called on the
// cocos thread; it intentionally uses Geometry Dash's object list, trigger
// graph, movement and collision callbacks rather than gd-sim.
VerificationResult verifyInGame(GJGameLevel* level, std::vector<uint8_t> const& macro);

// Searches by advancing and checkpointing a real PlayLayer. Because Geometry
// Dash owns every state transition, this automatically covers dual players,
// every vehicle, runtime hitboxes, triggers, portals and modifier blocks.
std::vector<uint8_t> pathfindInGame(
    GJGameLevel* level,
    std::atomic_bool& stop,
    std::function<void(double)> const& progress
);
