#pragma once
#include <EffectObject.hpp>

enum class OrbType {
	Yellow,
	Blue,
	Pink,
	Red,
	Green,
	Black,
	/// Green dash orb: hold to travel along the ring's angle.
	Dash,
	/// Pink dash orb: same, but flips gravity first.
	DashPink,
	/// Spider orb: teleports to the surface the arrow points at and flips gravity.
	Spider,
};

struct Orb : public EffectObject {
	OrbType type;

	Orb(Vec2D size, std::unordered_map<int, std::string>&& fields);
	bool touching(Player const&) const override;
	void collide(Player&) const override;
};