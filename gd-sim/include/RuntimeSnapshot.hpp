#pragma once

#include <util.hpp>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

// Neutral runtime data copied from Geometry Dash on the cocos thread before
// gd-sim starts. The simulator deliberately does not infer behavior or hitbox
// dimensions from object IDs.
enum class RuntimeObjectKind : uint8_t {
    Solid,
    Hazard,
    Slope,
    Breakable,
    JumpPad,
    JumpOrb,
    GravityPortal,
    VehiclePortal,
    SizePortal,
    SpeedPortal,
    DualPortal,
    TeleportPortal,
    DashOrb,
    Modifier,
    Trigger,
    CollisionObject,
    Other,
};

enum class RuntimeVehicle : uint8_t {
    Cube,
    Ship,
    Ball,
    Ufo,
    Wave,
    Robot,
    Spider,
    Swing,
};

struct RuntimeObjectSnapshot {
    int uniqueID = 0;
    int editorObjectID = 0; // diagnostics only; never used for behavior lookup
    RuntimeObjectKind kind = RuntimeObjectKind::Other;
    // Numeric GameObjectType value supplied by the runtime. Unlike editor IDs,
    // this is a behavioral classification and is stable across visual variants.
    int runtimeType = 0;
    RuntimeVehicle vehicle = RuntimeVehicle::Cube;

    Vec2D position;
    Vec2D hitboxSize;
    Vec2D scale {1, 1};
    float rotation = 0;

    bool active = true;
    bool enabled = true;
    bool flipX = false;
    bool flipY = false;
    bool touchTriggered = false;
    bool spawnTriggered = false;
    bool multiTriggered = false;

    int targetGroup = 0;
    int secondaryGroup = 0;
    std::vector<int> groups;

    // Original key/value properties are retained for mechanics whose runtime
    // binding is not represented by a stable named field on every platform.
    std::unordered_map<int, std::string> properties;
};

struct RuntimePlayerSnapshot {
    RuntimeVehicle vehicle = RuntimeVehicle::Cube;
    Vec2D position;
    Vec2D hitboxSize {30, 30};
    double yVelocity = 0;
    float rotation = 0;
    float speed = 1;
    bool upsideDown = false;
    bool mini = false;
    bool platformer = false;
};

struct RuntimeLevelSnapshot {
    std::string rawLevelString;
    RuntimePlayerSnapshot player1;
    RuntimePlayerSnapshot player2;
    bool dual = false;
    uint64_t randomSeed = 0;
    float levelLength = 0;
    std::vector<RuntimeObjectSnapshot> objects;
};
