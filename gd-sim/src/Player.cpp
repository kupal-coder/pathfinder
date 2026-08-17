#include <Player.hpp>
#include <Level.hpp>
#include <Slope.hpp>
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

	if (button != pressed) {
		button = pressed;
		input = button;
		buffer = button;
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
	Entity({{0, 15}, {30, 30}, 0}), frame(1), timeElapsed(0), dead(false),
	vehicle(Vehicle::from(VehicleType::Cube)),
	ceiling(999999), floor(0), grounded(true),
	coyoteFrames(0), jumpsRemaining(0), acceleration(0), velocity(0),
	velocityOverride(false), button(false), input(false),
	vehicleBuffer(false), upsideDown(false), small(false),
	speed(1), slopeData({{}, 0, false}), roundVelocity(true) {}
