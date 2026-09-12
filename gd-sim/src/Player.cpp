#include <Player.hpp>
#include <Level.hpp>
#include <Slope.hpp>
#include <algorithm>
#include <cmath>
#include <climits>

Entity Player::innerHitbox() const {
	return {pos, Vec2D{9, 9}, 0};
}

Entity Player::unrotatedHitbox() const {
	return {pos, size, 0};
}

void Player::setVelocity(float v, bool override) {
	velocityOverride = override;
	// Being small commonly means velocity is 4/5 the original velocity.
	velocity = v * (small ? 0.8f : 1.0f);
	if (v != 0.0f)
		grounded = false;
}

Player const& Player::prevPlayer() const {
	return level->getState(frame - 1);
}

Player const* Player::nextPlayer() const {
	return level->currentFrame() <= frame ? nullptr : &level->getState(frame + 1);
}

/**
 * In Geometry Dash, velocity is stored as 1/54 of distance per second.
 * It is also rounded to the nearest thousandth after (almost) every operation.
 * This function accounts for that rounding to match GD's exact behavior.
 */
float roundVel(float velocity, bool upsideDown) {
	float sign = upsideDown ? -1.0f : 1.0f;
	float nVel = velocity / PHYS_VEL_UNIT * sign;
	float floored = std::floor(nVel);
	float frac = nVel - floored;
	if (frac != 0.0f) {
		// Round to 3 decimal places to match GD's internal rounding
		frac = std::round(frac * 1000.0f) / 1000.0f;
		nVel = floored + frac;
	}
	return nVel * PHYS_VEL_UNIT * sign;
}

void Player::preCollision(bool pressed) {
	pos.x += PHYS_SPEEDS[(int)speed] * dt;
	pos.y += grav(velocity) * dt;

	frame++;
	timeElapsed += dt;
	grounded = false;
	velocityOverride = false;
	gravityPortal = false;
	roundVelocity = true;
	hasHBlock = false;
	hasDBlock = false;
	hasJBlock = false;

	if (button != pressed) {
		button = pressed;
		input = button;
		buffer = button;
	}

	if (isDashing) {
		if (!input) {
			isDashing = false;
		} else {
			velocity = 0.0f;
			velocityOverride = true;
			grounded = false;
		}
	}

	for (auto& i : actions)
		i(*this);
	actions.clear();

	potentialSlopes.clear();

	// Downhill slopes snap you automatically
	if (slopeData.slope && slopeData.slope->gravOrient(*this) == 1) {
		grounded = true;
	}
}

void Player::spiderTeleport(bool reverseGravity) {
	if (reverseGravity) {
		upsideDown = !upsideDown;
	}

	float playerLeft = pos.x - size.x * 0.5f;
	float playerRight = pos.x + size.x * 0.5f;
	float currentY = pos.y;

	float targetY = upsideDown ? (ceiling > 0.0f ? ceiling - size.y * 0.5f : 300.0f) : (floor + size.y * 0.5f);
	float closestDist = 999999.0f;
	bool hitHazard = false;

	if (level != nullptr && level->geom && !level->geom->empty()) {
		auto const& sections = *level->geom;
		size_t sectionIdx = std::min(std::max(0, (int)(pos.x / Level::sectionSize)), (int)sections.size() - 1);
		size_t minSec = (sectionIdx > 0) ? sectionIdx - 1 : 0;
		size_t maxSec = std::min(sections.size() - 1, sectionIdx + 1);

		for (size_t sec = minSec; sec <= maxSec; ++sec) {
			for (auto const& obj : sections[sec]) {
				float objLeft = obj->pos.x - obj->size.x * 0.5f;
				float objRight = obj->pos.x + obj->size.x * 0.5f;

				// Overlap on X axis
				if (playerRight >= objLeft && playerLeft <= objRight) {
					if (upsideDown) {
						// Teleporting upwards towards ceiling
						float surfY = obj->pos.y - obj->size.y * 0.5f;
						if (surfY > currentY - 1.0f) {
							float dist = surfY - currentY;
							if (dist < closestDist) {
								if (obj->prio == 1) { // Block
									closestDist = dist;
									targetY = surfY - size.y * 0.5f;
									hitHazard = false;
								} else if (obj->prio == 2) { // Hazard
									closestDist = dist;
									targetY = surfY;
									hitHazard = true;
								}
							}
						}
					} else {
						// Teleporting downwards towards floor
						float surfY = obj->pos.y + obj->size.y * 0.5f;
						if (surfY < currentY + 1.0f) {
							float dist = currentY - surfY;
							if (dist < closestDist) {
								if (obj->prio == 1) { // Block
									closestDist = dist;
									targetY = surfY + size.y * 0.5f;
									hitHazard = false;
								} else if (obj->prio == 2) { // Hazard
									closestDist = dist;
									targetY = surfY;
									hitHazard = true;
								}
							}
						}
					}
				}
			}
		}
	}

	if (hitHazard) {
		pos.y = targetY;
		dead = true;
		return;
	}

	pos.y = targetY;
	velocity = 0.0f;
	velocityOverride = true;
	grounded = true;
	buffer = false;
}

void Player::postCollision() {
	// Size portal only affects hitbox size at the end of frame
	if (small != prevPlayer().small) {
		size = small ? (size * 0.6f) : (size / 0.6f);
	}

	if (gravBottom(*this) <= gravFloor() && !velocityOverride && velocity <= 0.0f) {
		pos.y = grav(gravFloor()) + grav(size.y / 2.0f);
		grounded = true;
		snapData.playerFrame = 0;
	}

	// Fell through ceiling, or hit floor
	if (pos.y > PHYS_DEATH_Y_HIGH || (upsideDown && getBottom() < floor)) {
		dead = true;
		return;
	}

	// Coyote frames 
	if (prevPlayer().gravBottom(*this) > prevPlayer().gravFloor() && upsideDown == prevPlayer().upsideDown && !grounded && velocity <= 0.0f) {
		if (prevPlayer().grounded && !prevPlayer().input)
			coyoteFrames = 0;
		coyoteFrames++;
	} else {
		// Nothing will check for coyote frames this high
		coyoteFrames = INT_MAX;
	}

	vehicle.update(*this);

	if (!velocityOverride) {
		float newVel = velocity + acceleration * dt;

		// Player will fall off blocks a frame faster than expected.
		if (!grounded && prevPlayer().grounded && ((!input && (prevPlayer().button || !button)) || buffer) && prevPlayer().gravBottom(*this) > prevPlayer().gravFloor() && size == prevPlayer().size) {
			pos.y += roundVel(prevPlayer().grav(prevPlayer().acceleration) * dt, prevPlayer().upsideDown) * dt;
			if (gravityPortal && vehicle.type != VehicleType::Ship)
				newVel = -newVel;
			if (velocity == 0.0f)
				newVel += roundVel(prevPlayer().acceleration * dt, upsideDown);
		}

		velocity = newVel;
	}

	// Ball movements are not rounded in GD. Probably a bug!
	if (roundVelocity)
		velocity = roundVel(velocity, upsideDown);

	if (slopeData.slope)
		slopeData.slope->calc(*this);

	// Ensure the player hasn't gone beyond the bounds of the vehicle
	vehicle.clamp(*this);
}

Player::Player() :
	Entity({{0, 15}, {30, 30}, 0}), vehicle(Vehicle::from(VehicleType::Cube)),
	level(nullptr), timeElapsed(0.0f), acceleration(0.0f), velocity(0.0f),
	ceiling(999999.0f), floor(0.0f), dt(PHYS_DT),
	slopeData({{}, 0.0f, false}), snapData({{0.0f, 0.0f}, {0.0f, 0.0f}, 0.0f}, 0),
	coyoteFrames(0), robotBoostTicks(0), speed(1), frame(1),
	dead(false), grounded(true), velocityOverride(false),
	button(false), input(false), buffer(false), vehicleBuffer(false),
	upsideDown(false), small(false), gravityPortal(false),
	roundVelocity(true), isRobotBoosting(false) {}
