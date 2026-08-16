#include <Portals.hpp>
#include <Player.hpp>
#include <Level.hpp>

/*
	Classic teleport portals come as a blue/orange pair sharing an x position.
	The blue half stores how far the player is moved on the y axis; the orange
	half is only a marker and does nothing on its own.

	2.2's target-based teleport portals use group ids to find their destination.
	Groups are not simulated, so those are reported as unsupported instead.
*/

TeleportPortal::TeleportPortal(Vec2D size, std::unordered_map<int, std::string>&& fields)
	: EffectObject(size, std::move(fields)) {
	yOffset = stod_def(fields[54], 0);
}

void TeleportPortal::collide(Player& p) const {
	EffectObject::collide(p);

	if (yOffset == 0)
		return;

	p.pos.y += yOffset;

	// Teleporting keeps velocity but the player is no longer resting on anything.
	p.grounded = false;
	p.snapData.playerFrame = 0;
}

DualPortal::DualPortal(Vec2D size, std::unordered_map<int, std::string>&& fields)
	: EffectObject(size, std::move(fields)) {
	enable = atoi(fields[1].c_str()) == 286;
}

void DualPortal::collide(Player& p) const {
	EffectObject::collide(p);

	// The second player is spawned/removed by Level once the frame is finished,
	// because it needs the player's final position for this frame.
	if (enable) {
		if (!p.second) {
			p.startDual = true;
			p.dualMirrorY = pos.y;
		}
	} else {
		p.stopDual = true;
	}
}
