#pragma once
#include <EffectObject.hpp>

enum class PadType {
	Yellow,
	Blue,
	Pink,
	Red,
};

struct Pad : public EffectObject {
	PadType type;

	Pad(Vec2D size, std::unordered_map<int, std::string>&& fields);
	std::unique_ptr<Object> clone() const override { return std::make_unique<Pad>(*this); }
	void collide(Player&) const override;
};