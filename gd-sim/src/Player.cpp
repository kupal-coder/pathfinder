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

void Player::setVelocity(double v, bool override) {
	velocityOverride = override;

	// Being small commonly means velocity is 4/5 the original velocity.
	velocity = v * (small ? 0.8 : 1);

	if (v != 0)
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
 * It is also rounded to the nearest hundredth after (almost) every operation.
 * This function accounts for that rounding
 */
double roundVel(double velocity, bool upsideDown) {
	double nVel = velocity / 54.0 * (upsideDown * 2 - 1);
	double floored = (int)nVel;
	if (nVel != floored) {
		nVel = (double)std::round((nVel - floored) * 1000.0) / 1000.0 + floored;
	}
	return nVel * 54.0 * (upsideDown * 2 - 1);
}

/**
 * Apply and clear the deferred actions queued during the previous frame.
 *
 * These were std::function closures; they are now a POD tag plus payload so
 * that Player stays trivially copyable. The behaviour of each branch is a
 * transcription of the closure it replaced.
 */
void Player::applyPending() {
	auto clearSlope = [this]() {
		slopeData.slope = {};
		slopeData.elapsed = 0;
		slopeData.snapDown = false;
	};

	for (auto const& action : pending) {
		switch (action.kind) {
			case PendingAction::Kind::ClearSlope:
				clearSlope();
				break;
			case PendingAction::Kind::SetVelocityClearSlope:
				velocity = action.value;
				clearSlope();
				break;
			case PendingAction::Kind::SetRoundedVelocityClearSlope:
				velocity = roundVel(action.value, upsideDown);
				clearSlope();
				break;
			case PendingAction::Kind::WaveSize:
				size = small ? Vec2D(6, 6) : Vec2D(10, 10);
				break;
			case PendingAction::Kind::None:
				break;
		}
	}

	pending.clear();
}

void Player::preCollision(bool pressed) {
	pos.x += player_speeds[(int)speed] * dt;
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

	applyPending();

	level->clearPotentialSlopes();

	// Downhill slopes snap you automatically
	if (slopeData.slope && slopeData.slope->gravOrient(*this) == 1) {
		grounded = true;
	}
}

void Player::postCollision() {
	// Size portal only affects hitbox size at the end of frame
	if (small != prevPlayer().small) {
		size = small ? (size * 0.6) : (size / 0.6);
	}

	if (gravBottom(*this) <= gravFloor() && !velocityOverride && velocity <= 0 ) {
		pos.y = grav(gravFloor()) + grav(size.y / 2);
		grounded = true;
		snapData.playerFrame = 0;
	}

	// Fell through ceiling, or hit floor
	if (pos.y > 1476.3 || (upsideDown && getBottom() < floor)) {
		dead = true;
		return;
	}

	// Coyote frames 
	if (prevPlayer().gravBottom(*this) > prevPlayer().gravFloor() && upsideDown == prevPlayer().upsideDown && !grounded && velocity <= 0) {
		if (prevPlayer().grounded && !prevPlayer().input)
			coyoteFrames = 0;
		coyoteFrames++;
	} else {
		// Nothing will check for coyote frames this high
		coyoteFrames = INT_MAX;
	}

	vehicle.update(*this);

	if (!velocityOverride) {
		double newVel = velocity + acceleration * dt;

		// Player will fall off blocks a frame faster than expected.
		if (!grounded && prevPlayer().grounded && ((!input && (prevPlayer().button || !button)) || buffer) && prevPlayer().gravBottom(*this) > prevPlayer().gravFloor() && size == prevPlayer().size) {
			pos.y += roundVel(prevPlayer().grav(prevPlayer().acceleration) * dt, prevPlayer().upsideDown) * dt;

			if (gravityPortal && vehicle.type != VehicleType::Ship)
				newVel = -newVel;

			if (velocity == 0)
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
	coyoteFrames(0), acceleration(0), velocity(0),
	velocityOverride(false), button(false), input(false),
	vehicleBuffer(false), upsideDown(false), small(false),
	speed(1), slopeData({{}, 0, false}), roundVelocity(true),
	// buffer, gravityPortal, dt and level were previously left uninitialised.
	// Reading them before assignment is undefined behaviour -- UBSan flags a
	// bool holding 127 -- and it made every state copy carry indeterminate
	// bytes, which would silently poison state hashing during search.
	buffer(false), gravityPortal(false), dt(1/240.f), level(nullptr),
	snapData({}) {}







