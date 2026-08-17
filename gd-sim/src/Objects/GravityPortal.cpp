#include <Portals.hpp>
#include <Player.hpp>

GravityPortal::GravityPortal(Vec2D size, std::unordered_map<int, std::string>&& fields) : EffectObject(size, std::move(fields)), upsideDown(atoi(fields[1].c_str()) == 11) {}


void GravityPortal::collide(Player& p) const {
	EffectObject::collide(p);

	if (upsideDown != p.upsideDown) {
		// Gravity portals halve the velocity
		p.velocity = -p.velocity / 2;
		p.upsideDown = upsideDown;
		p.gravityPortal = true;
	}
}
