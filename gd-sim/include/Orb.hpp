#pragma once
#include <EffectObject.hpp>

enum class OrbType {
	Yellow,
	Blue,
	Pink,
	Red,
	Green,
	Black,
	Dash,
};

struct Orb : public EffectObject {
	OrbType type;

	Orb(Vec2D size, std::unordered_map<int, std::string>&& fields);
	std::unique_ptr<Object> clone() const override { return std::make_unique<Orb>(*this); }
	bool touching(Player const&) const override;
	void collide(Player&) const override;
};