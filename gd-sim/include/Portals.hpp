#pragma once
#include <EffectObject.hpp>
#include <Vehicle.hpp>

struct VehiclePortal : public EffectObject {
    VehicleType type;
    VehiclePortal(Vec2D size, std::unordered_map<int, std::string>&& fields);
    std::unique_ptr<Object> clone() const override { return std::make_unique<VehiclePortal>(*this); }
    void collide(Player&) const override;
};

struct GravityPortal : public EffectObject {
    bool upsideDown;
    GravityPortal(Vec2D size, std::unordered_map<int, std::string>&& fields);
    std::unique_ptr<Object> clone() const override { return std::make_unique<GravityPortal>(*this); }
    void collide(Player&) const override;
};

struct SizePortal : public EffectObject {
    bool small;
    SizePortal(Vec2D size, std::unordered_map<int, std::string>&& fields);
    std::unique_ptr<Object> clone() const override { return std::make_unique<SizePortal>(*this); }
    void collide(Player&) const override;
};

struct SpeedPortal : public EffectObject {
    int speed;
    SpeedPortal(Vec2D size, std::unordered_map<int, std::string>&& fields);
    std::unique_ptr<Object> clone() const override { return std::make_unique<SpeedPortal>(*this); }
    void collide(Player&) const override;
    SpeedPortal const* asSpeedPortal() const override { return this; }
};

struct TeleportPortal : public EffectObject {
    int groupId;
    TeleportPortal* linkedPortal; // nullptr if not linked

    TeleportPortal(Vec2D size, std::unordered_map<int, std::string>&& fields);
    std::unique_ptr<Object> clone() const override { return std::make_unique<TeleportPortal>(*this); }
    void collide(Player&) const override;
    TeleportPortal* asTeleportPortal() override { return this; }
};
