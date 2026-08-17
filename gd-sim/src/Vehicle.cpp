#include <Vehicle.hpp>
#include <Player.hpp>
#include <Object.hpp>
#include <Physics.hpp>
#include <algorithm>
#include <Slope.hpp>
#include <cfloat>
#include <cmath>

/// When the cube lands, it's rotation is snapped to whichever 90 degree angle is closest
float normalizeRotation(Player const& p, float angle) {
    float playerRotation = (int)p.rotation % 360;
    float diff = std::fmod(playerRotation - angle, 90.0f);
    if (diff < 0)
        diff += 90.0f;
    if (std::abs(playerRotation - angle) < std::abs(diff))
        return angle;
    else
        return playerRotation - diff;
}

/// Ship, UFO, Wave all have the same basic mechanism for rotation
void rotateFly(Player& p, float mult) {
	auto diff = p.pos - p.prevPlayer().pos;
	// Unknown why this happens
	if (p.dt * 72.0f <= std::pow(diff.x, 2) + std::pow(diff.y, 2)) {
		p.rotation = slerp(p.rotation * 0.017453292f, atan2(diff.y, diff.x), (p.dt * 60.0f) * mult) * 57.29578f;
	}
}

Vehicle cube() {
	Vehicle v;
	v.type = VehicleType::Cube;

	v.enter = +[](Player& p) {
		if (p.prevPlayer().vehicle.type != VehicleType::Ball)
			p.velocity = p.velocity / 2.0f;
		if (p.prevPlayer().vehicle.type == VehicleType::Wave)
			p.velocity = p.velocity / 2.0f;
		if (p.prevPlayer().vehicle.type == VehicleType::Ship && p.input)
			p.buffer = true;
	};

	v.clamp = +[](Player& p) {
		if (p.velocity < PHYS_CUBE_MAX_FALL)
			p.velocity = PHYS_CUBE_MAX_FALL;
		if (p.gravTop(p.innerHitbox()) >= p.gravCeiling())
			p.dead = true;
	};

	v.update = +[](Player& p) {
		p.acceleration = PHYS_CUBE_GRAVITY[p.speed];

		// Strange hardcode
		if (p.gravityPortal && p.grav(p.velocity) > 350.0f && p.speed > 1) {
			p.acceleration -= 6.48f;
		}

		p.rotation = 0;
		bool jump = false;

		if (p.grounded) {
			if (p.input) {
				jump = true;
			} else {
				p.setVelocity(0.0f, true);
			}
			p.buffer = false;
		}

		if (p.upsideDown && p.input && p.coyoteFrames < PHYS_COYOTE_FRAMES_NORMAL) {
			jump = true;
			p.buffer = false;
		}

		if (jump) {
			float jumpVel = PHYS_CUBE_JUMP[p.speed];

			// On slopes, you jump higher depending on how long you've been on the slope
			if (p.slopeData.slope && p.slopeData.slope->orientation == 0) {
				float time = std::clamp(10.0f * (p.timeElapsed - p.slopeData.elapsed), 0.4f, 1.0f);
				auto slope = p.slopeData.slope;
				float vel = 0.9f * std::min(1.12f / slope->angle(), 1.54f) * (slope->size.y * PHYS_SPEEDS[p.speed] / slope->size.x);
				p.setVelocity(0.25f * time * vel + jumpVel, p.prevPlayer().input);
				p.grounded = false;
			} else {
				p.setVelocity(jumpVel, p.prevPlayer().input);
				p.grounded = false;
			}
		}
	};

	v.bounds = FLT_MAX;
	return v;
}

Vehicle ship() {
	Vehicle v;
	v.type = VehicleType::Ship;

	v.enter = +[](Player& p) {
		VehicleType pv = p.prevPlayer().vehicle.type;
		if (pv == VehicleType::Ufo || pv == VehicleType::Wave)
			p.velocity = p.velocity / 4.0f;
		else 
			p.velocity = p.velocity / 2.0f;
	};

	v.clamp = +[](Player& p) {
		// Can't buffer clicks on ship
		p.buffer = false;

		// Max velocity
		float maxRise = p.small ? PHYS_SHIP_MAX_RISE_SMALL : PHYS_SHIP_MAX_RISE_NORM;
		float maxFall = p.small ? PHYS_SHIP_MAX_FALL_SMALL : PHYS_SHIP_MAX_FALL_NORM;
		p.velocity = std::clamp(p.velocity, maxFall, maxRise);

		if (p.gravTop(p) > p.gravCeiling()) {
			if (p.velocity > 0.0f) {
				p.setVelocity(0.0f, false);
			}
			p.pos.y = p.grav(p.gravCeiling()) - p.grav(p.size.y / 2.0f);
		}
	};

	v.update = +[](Player& p) {
		p.buffer = false;
	
		if (p.grounded)
			p.setVelocity(0.0f, !p.input);

		float threshold = p.grav(PHYS_VELOCITY_THRESHOLDS[p.speed]);

		if (p.input) {
			if (p.velocity <= threshold)
				p.acceleration = p.small ? PHYS_SHIP_ACCEL_RISE_HIGH_SMALL : PHYS_SHIP_ACCEL_RISE_HIGH_NORM;
			else
				p.acceleration = p.small ? PHYS_SHIP_ACCEL_RISE_LOW_SMALL : PHYS_SHIP_ACCEL_RISE_LOW_NORM;
		} else {
			if (p.velocity >= threshold)
				p.acceleration = p.small ? PHYS_SHIP_ACCEL_FALL_HIGH_SMALL : PHYS_SHIP_ACCEL_FALL_HIGH_NORM;
			else
				p.acceleration = p.small ? PHYS_SHIP_ACCEL_FALL_LOW_SMALL : PHYS_SHIP_ACCEL_FALL_LOW_NORM;
		}

		if (p.grav(p.pos.y) >= p.gravCeiling()) {
			p.setVelocity(0.0f, false);
		}

		rotateFly(p, 0.15f);
	};

	v.bounds = 300.0f;
	return v;
}

Vehicle ball() {
	Vehicle v;
	v.type = VehicleType::Ball;

	v.clamp = +[](Player& p) {
		p.velocity = std::clamp(p.velocity, -PHYS_BALL_MAX_VEL, PHYS_BALL_MAX_VEL);
		if (p.grav(p.pos.y) >= p.gravCeiling() && p.velocity > 0.0f) {
			p.setVelocity(0.0f, true);
			if (p.input)
				p.upsideDown = !p.upsideDown;
		}
	};

	v.enter = +[](Player& p) {
		if (p.input)
			p.vehicleBuffer = true;
		VehicleType pv = p.prevPlayer().vehicle.type;
		switch (pv) {
			case VehicleType::Ship:
			case VehicleType::Ufo:
				p.velocity = p.velocity / 2.0f;
				break;
			default: break;
		}
	};

	v.update = +[](Player& p) {
		if (!p.prevPlayer().velocityOverride || p.prevPlayer().slopeData.slope)
			p.acceleration = PHYS_BALL_GRAVITY;

		if (!p.input)
			p.vehicleBuffer = false;

		bool jump = false;

		if (p.grounded) {
			if (p.input && (p.prevPlayer().buffer || !p.prevPlayer().input || p.vehicleBuffer)) {
				jump = true;
			} else {
				p.setVelocity(0.0f, true);
			}
			p.buffer = false;
		} else if (p.buffer && p.coyoteFrames < (p.upsideDown ? PHYS_COYOTE_FRAMES_BALL : 1)) {
			jump = true;
		}

		if (jump) {
			float newVel = PHYS_BALL_JUMP[p.speed];

			if (p.slopeData.slope && p.slopeData.slope->orientation == 0) {
				auto slope = p.slopeData.slope;
				// Ball doesn't round, leading to very silly results
				newVel -= p.grav(0.300000001f * roundVel(0.16875f * std::min(1.12f / slope->angle(), 1.54f) * (slope->size.y * PHYS_SPEEDS[p.speed] / slope->size.x), p.upsideDown));
			}

			p.upsideDown = !p.upsideDown;
			p.setVelocity(newVel, p.prevPlayer().buffer || p.vehicleBuffer);
			p.vehicleBuffer = false;
			p.buffer = false;
			p.input = false;
			p.roundVelocity = false;
		}
	};

	v.bounds = 240.0f;
	return v;
}

Vehicle ufo() {
	Vehicle v;
	v.type = VehicleType::Ufo;

	v.enter = +[](Player& p) {
		VehicleType pv = p.prevPlayer().vehicle.type;
		if ((pv == VehicleType::Ship || pv == VehicleType::Wave) && p.input)
			p.buffer = true;
		p.velocity = p.velocity / (pv == VehicleType::Ship ? 4.0f : 2.0f);
	};

	v.clamp = +[](Player& p) {
		float maxRise = p.small ? PHYS_UFO_MAX_RISE_SMALL : PHYS_UFO_MAX_RISE_NORM;
		float maxFall = p.small ? PHYS_UFO_MAX_FALL_SMALL : PHYS_UFO_MAX_FALL_NORM;
		p.velocity = std::clamp(p.velocity, maxFall, maxRise);

		p.input = p.button;

		if (p.gravTop(p) > p.gravCeiling()) {
			if (p.velocity > 0.0f) {
				p.setVelocity(0.0f, false);
			}
			p.pos.y = p.grav(p.gravCeiling()) - p.grav(p.size.y / 2.0f);
		}
	};

	v.update = +[](Player& p) {
		if (p.buffer) {
			// UFO jump: fixed upward velocity on buffered click
			p.velocity = std::max(p.velocity, p.small ? PHYS_UFO_JUMP_SMALL : PHYS_UFO_JUMP_NORM);
			p.velocityOverride = true;
			p.buffer = false;
			p.grounded = false;
		} else {
			float threshold = p.grav(PHYS_VELOCITY_THRESHOLDS[p.speed]);

			if (p.velocity > threshold) {
				p.acceleration = p.small ? PHYS_UFO_GRAV_HIGH_SMALL : PHYS_UFO_GRAV_HIGH_NORM;
			} else {
				p.acceleration = p.small ? PHYS_UFO_GRAV_LOW_SMALL : PHYS_UFO_GRAV_LOW_NORM;
			}

			if (p.grounded) {
				p.setVelocity(0.0f, true);
			}

			if (p.button)
				p.input = false;
		}
	};

	v.bounds = 300.0f;
	return v;
}

Vehicle wave() {
	Vehicle v;
	v.type = VehicleType::Wave;

	v.enter = +[](Player& p) {
		p.actions.push_back(+[](Player& p) {
			p.size = p.small ? Vec2D(6, 6) : Vec2D(10, 10);
		});
	};

	v.clamp = +[](Player& p) {
		float waveTop = p.grav(p.pos.y + p.grav(p.size.y));
		float waveBottom = p.grav(p.pos.y - p.grav(p.size.y));
		p.velocity = (p.input * 2.0f - 1.0f) * PHYS_SPEEDS[p.speed] * (p.small ? 2.0f : 1.0f);

		if (waveBottom <= p.gravFloor()) {
			p.pos.y = p.grav(p.gravFloor() + p.size.y);
			if (waveBottom == p.gravFloor() && !p.input)
				p.velocity = 0.0f;
		} else if (waveTop >= p.gravCeiling()) {
			p.pos.y = p.grav(p.gravCeiling() - p.size.y);
			if (waveTop == p.gravCeiling() && p.input)
				p.velocity = 0.0f;
		}
	};

	v.update = +[](Player& p) {
		p.acceleration = 0.0f;
		p.buffer = false;
		rotateFly(p, p.small ? 0.4f : 0.25f);
	};

	v.bounds = 300.0f;
	return v;
}

Vehicle Vehicle::from(VehicleType v) {
	switch (v) {
		case VehicleType::Cube:
			return cube();
		case VehicleType::Ship:
			return ship();
		case VehicleType::Ball:
			return ball();
		case VehicleType::Ufo:
			return ufo();
		case VehicleType::Wave:
			return wave();
	}
}
