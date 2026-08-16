#include <Vehicle.hpp>
#include <Player.hpp>
#include <Level.hpp>
#include <Object.hpp>
#include <algorithm>
#include <Slope.hpp>
#include <cfloat>
#include <cmath>

/*
	For ship and ufo, there are two sets of acceleration values depending on the current
	velocity. If the current velocity is higher than one of these thresholds
	(indexed by speed) then a lighter acceleration would be used.
*/
constexpr double velocity_thresholds[] = {
	101.541492,
	103.485494592,
	103.377492,
	103.809492,
	103.809492
};

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
	if (p.dt * 72 <= std::pow(diff.x, 2) + std::pow(diff.y, 2)) {
		p.rotation = slerp(p.rotation * 0.017453292f, atan2(diff.y, diff.x), (p.dt * 60) * mult) * 57.29578f;
	}
}

Vehicle cube() {
	Vehicle v;
	 v.type = VehicleType::Cube;

	v.enter = +[](Player& p) {
		if (p.prevPlayer().vehicle.type != VehicleType::Ball)
			p.velocity = p.velocity / 2;

		if (p.prevPlayer().vehicle.type == VehicleType::Wave)
			p.velocity = p.velocity / 2;

		if (p.prevPlayer().vehicle.type == VehicleType::Ship && p.input)
			p.buffer = true;
	};

	v.clamp = +[](Player& p) {
		if (p.velocity < -810)
			p.velocity = -810;

		if (p.gravTop(p.innerHitbox()) >= p.gravCeiling())
			p.dead = true;
	};

	v.update = +[](Player& p) {
		// Fall speeds
		static double accelerations[] = {
			-2747.52,
			-2794.1082, 
			-2786.4,
			-2799.36,
			-2799.36
		};
		p.acceleration = accelerations[p.speed];

		// Strange hardcode
		if (p.gravityPortal && p.grav(p.velocity) > 350 && p.speed > 1) {
			p.acceleration -= 6.48;
		}

		//TODO add rotation
		p.rotation = 0;

		bool jump = false;

		if (p.grounded) {
			if (p.input) {
				jump = true;
			} else {
				p.setVelocity(0, true);
			}
			p.buffer = false;
		}

		if (p.upsideDown && p.input && p.coyoteFrames < 10) {
			jump = true;
			p.buffer = false;
		}

		if (jump) {
			static double jumpHeights[] = {
				573.481728,
				603.7217172,
				616.681728,
				606.421728,
				606.421728
			};

			// On slopes, you jump higher depending on how long you've been on the slope
			if (p.slopeData.slope && p.slopeData.slope->orientation == 0) {
				auto time = std::clamp(10 * (p.timeElapsed - p.slopeData.elapsed), 0.4, 1.0);
				double vel = 0.9 * std::min(1.12 / p.slopeData.slope->angle(), 1.54) * (p.slopeData.slope->size.y * player_speeds[p.speed] / p.slopeData.slope->size.x);
				p.setVelocity(0.25 * time * vel + jumpHeights[p.speed], p.prevPlayer().input);

				//p.velocity = std::floor(1000 * p.velocity / 54.) * 54 / 1000.;
				p.grounded = false;
			} else {
				p.setVelocity(jumpHeights[p.speed], p.prevPlayer().input);
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
		if (p.prevPlayer().vehicle.type == VehicleType::Ufo || p.prevPlayer().vehicle.type == VehicleType::Wave)
			p.velocity = p.velocity / 4.0;
		else 
			p.velocity = p.velocity / 2.0;
	};

	v.clamp = +[](Player& p) {
		// Can't buffer clicks on ship
		p.buffer = false;

		// Max velocity
		p.velocity = std::clamp(p.velocity, 
			p.small ? -406.566 : -345.6,
			p.small ? 508.248 : 432.0
		);

		//TODO deal with slope velocity more consistently

		// Slopes complicate things like "maximum velocity"
		/*if (p.input)
			p.velocity = std::min(p.velocity, p.small ? 508.248 : 432.0);
		else
			p.velocity = std::max(p.velocity, p.small ? -406.566 : -345.6);*/


		if (p.gravTop(p) > p.gravCeiling()) {
			if (p.velocity > 0) {
				p.setVelocity(0, false);
			}
			p.pos.y = p.grav(p.gravCeiling()) - p.grav(p.size.y / 2);
		}
	};

	v.update = +[](Player& p) {
		p.buffer = false;
	
		if (p.grounded)
			p.setVelocity(0, !p.input);

		if (p.input) {
			if (p.velocity <= p.grav(velocity_thresholds[p.speed]))
				p.acceleration = p.small ? 1643.5872 : 1397.0491;
			else
				p.acceleration = p.small ? 1314.86976 : 1117.64328;
		} else {
			if (p.velocity >= p.grav(velocity_thresholds[p.speed]))
				p.acceleration = p.small ? -1577.85408 : -1341.1719;
			else
				p.acceleration = p.small ? -1051.8984 : -894.11464;
		}

		if (p.grav(p.pos.y) >= p.gravCeiling()) {
			p.setVelocity(0, false);
		}

		rotateFly(p, 0.15f);
	};

	v.bounds = 300;

	return v;
}

Vehicle ball() {
	Vehicle v;

	v.type = VehicleType::Ball;
	v.clamp = +[](Player& p) {
		if (p.velocity >= 810)
			p.velocity = 810;
		if (p.velocity <= -810)
			p.velocity = -810;

		if (p.grav(p.pos.y) >= p.gravCeiling() && p.velocity > 0) {
			p.setVelocity(0, true);

			if (p.input)
				p.upsideDown = !p.upsideDown;
		}
	};

	v.enter = +[](Player& p) {
		if (p.input)
			p.vehicleBuffer = true;

		switch (p.prevPlayer().vehicle.type) {
			case VehicleType::Ship:
			case VehicleType::Ufo:
				p.velocity = p.velocity / 2;
				break;
			default: break;
		}
	};

	v.update = +[](Player& p) {
		if (!p.prevPlayer().velocityOverride || p.prevPlayer().slopeData.slope)
			p.acceleration = -1676.46672;

		if (!p.input)
			p.vehicleBuffer = false;

		bool jump = false;

		if (p.grounded) {
			//p.rotVelocity = p.grav(600);
			if (p.input && (p.prevPlayer().buffer || !p.prevPlayer().input || p.vehicleBuffer)) {
				jump = true;
			} else {
				p.setVelocity(0, true);
			}
			p.buffer = false;
		} else if (p.buffer && p.coyoteFrames < (p.upsideDown ? 16 : 1)) {
			jump = true;
		}

		if (jump) {
			static double jumpHeights[] = {
				-172.044007,
				-181.11601,
				-185.00401,
				-181.92601,
				-181.92601
			};

			double newVel = jumpHeights[p.speed];

			if (p.slopeData.slope && p.slopeData.slope->orientation == 0) {
				auto slope = p.slopeData.slope;

				// Ball doesn't round, leading to very silly results
				newVel -= p.grav(0.300000001 * roundVel(0.16875 * std::min(1.12 / slope->angle(), 1.54) * (slope->size.y * player_speeds[p.speed] / slope->size.x), p.upsideDown));
			}

			p.upsideDown = !p.upsideDown;
			p.setVelocity(newVel, p.prevPlayer().buffer || p.vehicleBuffer);
			p.vehicleBuffer = false;
			p.buffer = false;
			p.input = false;
			p.roundVelocity = false;
		}
	};

	v.bounds = 240;

	return v;
}

Vehicle ufo() {
	Vehicle v;

	v.type = VehicleType::Ufo;
	v.enter = +[](Player& p) {
		VehicleType pv = p.prevPlayer().vehicle.type;
		if ((pv == VehicleType::Ship || pv == VehicleType::Wave) && p.input)
			p.buffer = true;

		p.velocity = p.velocity / (p.prevPlayer().vehicle.type == VehicleType::Ship ? 4 : 2);
	};

	v.clamp = +[](Player& p) {
		p.velocity = std::clamp(p.velocity, 
			p.small ? -406.56 : -345.6,
			p.small ? 508.24 : 432.0
		);

		p.input = p.button;

		if (p.gravTop(p) > p.gravCeiling()) {
			if (p.velocity > 0) {
				p.setVelocity(0, false);
			}
			p.pos.y = p.grav(p.gravCeiling()) - p.grav(p.size.y / 2);
		}

	};

	v.update = +[](Player& p) {
		if (p.buffer) {
			p.velocity = std::max(p.velocity, p.small ? 358.992 : 371.034);
			p.velocityOverride = true;
			p.buffer = false;
			p.grounded = false;
		} else {
			if (p.velocity > p.grav(velocity_thresholds[p.speed])) {
				p.acceleration = p.small ? -1969.92 : -1671.84;
			} else {
				p.acceleration = p.small ? -1308.96 : -1114.56;
			}

			if (p.grounded) {
				p.setVelocity(0, true);
			}

			if (p.button)
				p.input = false;
		}
	};

	v.bounds = 300;

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
		float waveTop = p.grav(p.pos.y + p.grav(p.size.y)) ;
		float waveBottom = p.grav(p.pos.y - p.grav(p.size.y));

		p.velocity = (p.input * 2 - 1) * player_speeds[p.speed] * (p.small ? 2 : 1);

		if (waveBottom <= p.gravFloor()) {
			p.pos.y = p.grav(p.gravFloor() + p.size.y);
			if (waveBottom == p.gravFloor() && !p.input)
				p.velocity = 0;
		} else if (waveTop >= p.gravCeiling()) {
			p.pos.y = p.grav(p.gravCeiling() - p.size.y);
			if (waveTop == p.gravCeiling() && p.input)
				p.velocity = 0;
		}

		//p.rotation = (p.input ? 1 : -1) * (p.small ? 63.4258423 : 45);
	};
	v.update = +[](Player& p) {
		p.acceleration = 0;
		p.buffer = false;

		rotateFly(p, p.small ? 0.4 : 0.25);
	};

	v.bounds = 300;

	return v;
}


/*
	Robot, spider and swing were added after this simulator was written, so their
	constants are derived from the cube/ship values and the documented jump
	heights rather than dumped from the game. They are close enough for the
	pathfinder to solve with, and every macro is replay-verified before export.
*/

/// How long the robot keeps thrusting while the button is held after a jump.
constexpr int robot_hold_frames = 32;

Vehicle robot() {
	Vehicle v;
	v.type = VehicleType::Robot;

	v.enter = +[](Player& p) {
		if (p.prevPlayer().vehicle.type != VehicleType::Ball)
			p.velocity = p.velocity / 2;

		p.robotJumpFrame = -1000;
	};

	v.clamp = +[](Player& p) {
		if (p.velocity < -810)
			p.velocity = -810;

		if (p.gravTop(p.innerHitbox()) >= p.gravCeiling())
			p.dead = true;
	};

	v.update = +[](Player& p) {
		static double accelerations[] = {
			-2747.52,
			-2794.1082,
			-2786.4,
			-2799.36,
			-2799.36
		};
		p.acceleration = accelerations[p.speed];
		p.rotation = 0;

		bool jump = false;

		if (p.grounded) {
			if (p.input) {
				jump = true;
			} else {
				p.setVelocity(0, true);
			}
			p.buffer = false;
		}

		if (jump) {
			// A tap is weaker than a cube jump; holding makes up the difference.
			static double jumpHeights[] = {
				458.785382,
				482.977374,
				493.345382,
				485.137382,
				485.137382
			};

			p.setVelocity(jumpHeights[p.speed], p.prevPlayer().input);
			p.grounded = false;
			p.robotJumpFrame = p.frame;
		} else if (p.input && p.velocity > 0 && p.frame - p.robotJumpFrame <= robot_hold_frames) {
			// Sustained jump: gravity is cut while the button is held.
			p.acceleration *= 0.4;
		}
	};

	v.bounds = FLT_MAX;

	return v;
}

Vehicle spider() {
	Vehicle v;
	v.type = VehicleType::Spider;

	v.enter = +[](Player& p) {
		if (p.prevPlayer().vehicle.type != VehicleType::Ball)
			p.velocity = p.velocity / 2;
	};

	v.clamp = +[](Player& p) {
		if (p.velocity < -810)
			p.velocity = -810;
		if (p.velocity > 810)
			p.velocity = 810;
	};

	v.update = +[](Player& p) {
		static double accelerations[] = {
			-2747.52,
			-2794.1082,
			-2786.4,
			-2799.36,
			-2799.36
		};
		p.acceleration = accelerations[p.speed];
		p.rotation = 0;

		// A press while on a surface teleports to the nearest surface overhead
		// and inverts gravity, instead of jumping.
		bool press = p.input && (!p.prevPlayer().button || p.prevPlayer().buffer);

		if (p.grounded && press) {
			if (auto target = p.level->spiderTarget(p))
				p.pos.y = *target;
			else
				p.pos.y = p.grav(p.gravCeiling()) - p.grav(p.size.y / 2);

			p.upsideDown = !p.upsideDown;
			p.setVelocity(0, true);
			p.grounded = true;
			p.buffer = false;
			p.input = false;
		} else if (p.grounded) {
			p.setVelocity(0, true);
			p.buffer = false;
		}
	};

	v.bounds = 270;

	return v;
}

Vehicle swing() {
	Vehicle v;
	v.type = VehicleType::Swing;

	v.enter = +[](Player& p) {
		VehicleType pv = p.prevPlayer().vehicle.type;
		if (pv == VehicleType::Ufo || pv == VehicleType::Wave)
			p.velocity = p.velocity / 4.0;
		else
			p.velocity = p.velocity / 2.0;
	};

	v.clamp = +[](Player& p) {
		p.buffer = false;

		p.velocity = std::clamp(p.velocity,
			p.small ? -406.566 : -345.6,
			p.small ? 508.248 : 432.0
		);

		if (p.gravTop(p) > p.gravCeiling()) {
			if (p.velocity > 0)
				p.setVelocity(0, false);
			p.pos.y = p.grav(p.gravCeiling()) - p.grav(p.size.y / 2);
		}
	};

	v.update = +[](Player& p) {
		p.buffer = false;

		// Every press flips gravity; there is no thrust and no need to be grounded.
		if (p.button && !p.prevPlayer().button) {
			p.upsideDown = !p.upsideDown;
			p.velocity = -p.velocity;
		}

		p.acceleration = p.small ? -1643.5872 : -1397.0491;

		if (p.grounded)
			p.setVelocity(0, true);

		rotateFly(p, 0.15f);
	};

	v.bounds = 300;

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
		case VehicleType::Robot:
			return robot();
		case VehicleType::Spider:
			return spider();
		case VehicleType::Swing:
			return swing();
	}

	return cube();
}