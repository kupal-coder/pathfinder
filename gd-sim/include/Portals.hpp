#pragma once

#include <EffectObject.hpp>
#include <Vehicle.hpp>

#include <unordered_map>
#include <string>

// Prevent MSVC Windows RPC macro collision with 'small'
#ifdef small
#undef small
#endif

// Forward declaration
struct Player;

// =============================================================================
// Vehicle Mode Portal (Cube, Ship, Ball, UFO, Wave, Robot, Spider, Swing)
// =============================================================================
struct VehiclePortal : public EffectObject {
    VehicleType type = VehicleType::Cube;

    VehiclePortal(Vec2D size, std::unordered_map<int, std::string>&& fields);
    void collide(Player& player) const override;
};

// =============================================================================
// Gravity Portal (Blue = Normal, Yellow = Upside-Down)
// =============================================================================
struct GravityPortal : public EffectObject {
    bool upsideDown = false;

    GravityPortal(Vec2D size, std::unordered_map<int, std::string>&& fields);
    void collide(Player& player) const override;
};

// =============================================================================
// Size Portal (Normal / Mini)
// =============================================================================
struct SizePortal : public EffectObject {
    bool small = false;

    SizePortal(Vec2D size, std::unordered_map<int, std::string>&& fields);
    void collide(Player& player) const override;
};

// =============================================================================
// Speed Portal (0.5x, 1.0x, 2.0x, 3.0x, 4.0x)
// =============================================================================
struct SpeedPortal : public EffectObject {
    int speed = 1; // 0 = 0.5x, 1 = 1x, 2 = 2x, 3 = 3x, 4 = 4x

    SpeedPortal(Vec2D size, std::unordered_map<int, std::string>&& fields);
    void collide(Player& player) const override;
};

// =============================================================================
// Teleport Portal (Orange / Blue Portal Pairs)
// =============================================================================
struct TeleportPortal : public EffectObject {
    int groupId                         = 0;
    mutable TeleportPortal* linkedPortal = nullptr; // nullptr if not linked yet
    mutable int cooldown                = 0;       // frames to wait before re-teleporting

    TeleportPortal(Vec2D size, std::unordered_map<int, std::string>&& fields);
    void collide(Player& player) const override;

    TeleportPortal* asTeleportPortal() override { return this; }
    TeleportPortal const* asTeleportPortal() const override { return this; }
};

// =============================================================================
// Dual Portal (Orange = Dual Mode, Blue = Single Mode)
// =============================================================================
struct DualPortal : public EffectObject {
    bool dual = false;

    DualPortal(Vec2D size, std::unordered_map<int, std::string>&& fields);
    void collide(Player& player) const override;
};

// =============================================================================
// Mirror Portal (Yellow = Flipped Screen, Blue = Normal Screen)
// =============================================================================
struct MirrorPortal : public EffectObject {
    bool flipped = false;

    MirrorPortal(Vec2D size, std::unordered_map<int, std::string>&& fields);
    void collide(Player& player) const override;
};