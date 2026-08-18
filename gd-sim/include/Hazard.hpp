#pragma once
#include <Object.hpp>

struct Hazard : public Object {
    Hazard(Vec2D size, std::unordered_map<int, std::string>&& fields);
    std::unique_ptr<Object> clone() const override { return std::make_unique<Hazard>(*this); }
    void collide(Player&) const override;
};

struct Sawblade : public Hazard {
    using Hazard::Hazard;
    std::unique_ptr<Object> clone() const override { return std::make_unique<Sawblade>(*this); }
    bool touching(Player const&) const override;
};
