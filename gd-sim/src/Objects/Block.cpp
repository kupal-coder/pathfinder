#include <Block.hpp>
#include <Slope.hpp>
#include <Level.hpp>
#include <Player.hpp>
#include <cmath>
#include <array>
#include <algorithm>

Block::Block(Vec2D s, std::unordered_map<int, std::string>&& fields) : Object(s, std::move(fields)) {
	// Blocks have a prio of 1, so they are processed later than most other objects.
	prio = 1;

	// For 90° rotations: swap width/height, keep axis-aligned
	int r = static_cast<int>(fabs(rotation)) % 360;
	if (r == 90 || r == 270) {
		size = {size.y, size.x};
		rotation = 0.0f;
	} else if (r == 0 || r == 180) {
		// 0° and 180° are already axis-aligned
		rotation = 0.0f;
	}
	// For partial rotations (15°, 30°, 45°, etc.): PRESERVE rotation value

	// Edge case for this specific block.
	if (fields[1] == "468" && size.y == 5) {
		size.y -= 3.5;
	}
}

/**
 * "Snapping" here refers to what happens when the cube player is
 * holding a button on given block staircases. In order to prevent
 * the player from falling off a block staircase, the X position
 * is manually adjusted by a set amount. The amount depends on
 * the type of staircase, speed, and cube size.
 */
enum class SnapType {
	None,
	BigStair,
	LittleStair,
	DownStair
};

float snapThreshold(Vec2D const& diff, Player const& p) {
	std::array<Vec2D, 3> stairs;
	float threshold;
	switch (p.speed) {
	case 0:
		stairs = { Vec2D(120, -30), Vec2D(90, 30), Vec2D(60, 60) };
		threshold = 1;
		break;
	case 1:
		stairs = { Vec2D(150, -30), Vec2D(p.small ? 90 : 120, 30), Vec2D(90, 60) };
		threshold = 1;
		break;
	case 2:
		stairs = { Vec2D(180, -30), Vec2D(p.small ? 90 : 150, 30), Vec2D(120, 60) };
		threshold = 2;
		break;
	case 3:
		stairs = { Vec2D(225, -30), Vec2D(p.small ? 90 : 180, 30), Vec2D(135, 60) };
		threshold = 2;
		break;
	default:
		stairs = { Vec2D(150, -30), Vec2D(120, 30), Vec2D(90, 60) };
		threshold = 1;
		break;
	}
	for (auto& stair : stairs) {
		if (std::abs(diff.x - stair.x) <= threshold && std::abs(diff.y - stair.y) <= threshold)
			return threshold;
	}
	return 0;
}

void trySnap(Block const& b, Player& p) {
	auto snapData = p.snapData;
	auto diff = b.pos - snapData.object.pos;
	diff.y = p.grav(diff.y);
	if (float threshold = snapThreshold(diff, p); threshold > 0) {
		p.pos.x = std::clamp(
			p.level->getState(snapData.playerFrame).nextPlayer()->pos.x + diff.x,
			p.pos.x - threshold,
			p.pos.x + threshold
		);
	}
}

void Block::collide(Player& p) const {
	// The maximum amount the player can dip below block while still being snapped up
	int clip = (p.vehicle.type == VehicleType::Ufo || p.vehicle.type == VehicleType::Ship) ? 7 : 10;

	// When hitting blue orbs/pads, there is a single frame where block collisions arent done.
	if (p.upsideDown != p.prevPlayer().upsideDown && !p.gravityPortal)
		return;

	// Check if this block is partially rotated (not axis-aligned)
	bool isRotated = (rotation != 0.0f);

	// Compute gravity-relative top and bottom of the block
	// For axis-aligned: use direct gravTop/gravBottom
	// For rotated: we'll handle via local-space transform in the landing section
	float blockGravTop = p.gravTop(*this);
	float blockGravBottom = p.gravBottom(*this);

	/*
		Going from slope to block means you have to check the bottom of the player
		adjusted for the angle of the slope
	*/
	float bottom = p.gravBottom(p);
	if (p.slopeData.slope) {
		if (p.slopeData.slope->angle() > 0.0f) {
			bottom = bottom + sin(p.slopeData.slope->angle()) * p.size.y / 2.0f;
			clip = 7;
			// Prevent block from catching on slope when it shouldn't
			if (blockGravTop - bottom < 2.0f) {
				return;
			}
		}
	}

	for (auto& entity : p.potentialSlopes) {
		auto block_comp = entity->orientation < 2 ? getTop() : getBottom();
		auto slope_comp = entity->orientation < 2 ? entity->getBottom() : entity->getTop();
		if (block_comp - slope_comp < 2.0f) {
			return;
		}
	}

	bool padHitBefore = (!p.prevPlayer().grounded && p.prevPlayer().velocity <= 0.0f && p.velocity > 0.0f);

	if (p.innerHitbox().intersects(*this)) {
		// Hitting block head-on — Entity::intersects already handles rotation via OBB
		p.dead = true;
	} else if (p.vehicle.type != VehicleType::Wave && blockGravTop - bottom <= clip && (padHitBefore || p.velocity <= 0.0f || p.gravityPortal)) {
		// Landing on top of the block
		if (isRotated) {
			// For rotated blocks: transform player into block's local (unrotated) space
			// to find the correct landing surface position
			Vec2D localPlayerPos = p.pos.rotate(-rotation, pos);

			// In local space, block is axis-aligned. Landing surface Y in local space:
			float localSurfaceY = p.upsideDown ? (pos.y - size.y / 2.0f) : (pos.y + size.y / 2.0f);

			// Create landing point in local space (same X as player, Y at surface)
			Vec2D localLandingPoint = Vec2D(localPlayerPos.x, localSurfaceY);

			// Transform back to world space
			Vec2D worldLandingPoint = localLandingPoint.rotate(rotation, pos);

			// Position player so their feet (or head, if upside-down) touch the surface
			p.pos.y = worldLandingPoint.y - p.grav(p.size.y / 2.0f);
		} else {
			// Axis-aligned: simple direct positioning
			p.pos.y = p.grav(blockGravTop) + p.grav(p.size.y / 2.0f);
		}

		// When hitting pads, the next frame will cause the player to slightly dip into the block
		if (!padHitBefore)
			p.grounded = true;

		// If on a downhill slope and hits a block, the player is no longer on that slope.
		if (p.slopeData.slope && p.slopeData.slope->angle() < 0.0f) {
			p.slopeData.slope = {};
		}

		// X-snapping — only for Cube on axis-aligned blocks
		if (p.vehicle.type == VehicleType::Cube && !isRotated) {
			if (!p.prevPlayer().grounded) {
				if (p.snapData.playerFrame > 0 && p.snapData.playerFrame + 1 < p.frame)
					trySnap(*this, p);
			}
			p.snapData.playerFrame = p.level->currentFrame();
			p.snapData.object = *this;
		}
	} else {
		// Ship, UFO, and Ball can hit the ceiling of a block without dying
		if (p.vehicle.type == VehicleType::Ship || p.vehicle.type == VehicleType::Ufo || p.vehicle.type == VehicleType::Ball) {
			float playerGravTop = p.gravTop(p);

			if (isRotated) {
				// For rotated blocks: use local-space transform for ceiling collision
				Vec2D localPlayerPos = p.pos.rotate(-rotation, pos);
				float localCeilingY = p.upsideDown ? (pos.y + size.y / 2.0f) : (pos.y - size.y / 2.0f);
				Vec2D localCeilingPoint = Vec2D(localPlayerPos.x, localCeilingY);
				Vec2D worldCeilingPoint = localCeilingPoint.rotate(rotation, pos);

				// Distance from player to ceiling surface
				float dist = p.upsideDown ? (worldCeilingPoint.y - p.pos.y) : (p.pos.y - worldCeilingPoint.y);
				if (dist <= clip - 1 && p.velocity > 0.0f) {
					p.pos.y = worldCeilingPoint.y + p.grav(p.size.y / 2.0f);
					p.velocity = 0.0f;
				}
			} else {
				if (playerGravTop - blockGravBottom <= clip - 1 && p.velocity > 0.0f) {
					p.pos.y = p.grav(blockGravBottom) - p.grav(p.size.y / 2.0f);
					p.velocity = 0.0f;
				}
			}
		}
	}
}
