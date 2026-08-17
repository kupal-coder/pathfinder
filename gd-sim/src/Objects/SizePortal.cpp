#include <Portals.hpp>
#include <Player.hpp>

SizePortal::SizePortal(Vec2D size, std::unordered_map<int, std::string>&& fields) : EffectObject(size, std::move(fields)), small(atoi(fields[1].c_str()) == 101) {}

/// Easiest portal!
void SizePortal::collide(Player& p) const {
	EffectObject::collide(p);

	p.small = small;
}
