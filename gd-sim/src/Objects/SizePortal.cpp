#include <Portals.hpp>
#include <Player.hpp>

SizePortal::SizePortal(Vec2D size, std::unordered_map<int, std::string>&& fields) : EffectObject(size, std::move(fields)), small(stoi_def(fields, 1) == 101) {}

/// Easiest portal!
void SizePortal::collide(Player& p) const {
	EffectObject::collide(p);

	p.small = small;
}
