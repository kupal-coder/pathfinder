#pragma once

#include <Object.hpp>
#include <unordered_map>
#include <string>

struct Player;

struct Hazard : public Object {
    Hazard(Vec2D size, std::unordered_map<int, std::string>&& fields);
    void collide(Player& player) const override;
};

struct Sawblade : public Hazard {
    using Hazard::Hazard;
    bool touching(Player const& player) const override;
};

struct Spike : public Hazard {
    using Hazard::Hazard;
    bool touching(Player const& player) const override;
};