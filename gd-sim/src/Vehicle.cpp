#include <Vehicle.hpp>
#include <Player.hpp>
#include <Object.hpp>
#include <Physics.hpp>
#include <Slope.hpp>
#include <algorithm>
#include <cfloat>
#include <cmath>

/// When the cube lands, its rotation is snapped to whichever 90-degree angle is closest
float normalizeRotation(Player const& p, float angle) {
    float playerRotation = std::fmod((float)((int)p.rotation % 360), 360.0f);
    float diff = std::fmod(playerRotation - angle, 90.0f);
    if (diff < 0.0f)
        diff += 90.0f;
    if (std::abs(playerRotation - angle) < std::abs(diff))
        return angle;
    else
        return playerRotation - diff;
}

/// Ship, UFO, Wave, and Swing all have the same basic mechanism for smooth rotation
void rotateFly(Player& p, float mult) {
    auto diff = p.pos - p.prevPlayer().pos;
    if (p.dt * 72.0f <= (diff.x * diff.x + diff.y * diff.y)) {
        p.rotation = slerp(p.rotation * 0.017453292f, std::atan2(diff.y, diff.x), (p.dt * 60.0f) * mult) * 57.29578f;
    }
}

// =============================================================================
// Cube
// =============================================================================
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

        // Specific GD engine gravity portal edge case
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

            // Slopes boost jump velocity depending on time spent on the slope
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

// =============================================================================
// Ship
// =============================================================================
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
        p.buffer = false;

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

// =============================================================================
// Ball
// =============================================================================
Vehicle ball() {
    Vehicle v;
    v.type = VehicleType::Ball;

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

    v.clamp = +[](Player& p) {
        p.velocity = std::clamp(p.velocity, -PHYS_BALL_MAX_VEL, PHYS_BALL_MAX_VEL);
        if (p.grav(p.pos.y) >= p.gravCeiling() && p.velocity > 0.0f) {
            p.setVelocity(0.0f, true);
            if (p.input)
                p.upsideDown = !p.upsideDown;
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

// =============================================================================
// UFO
// =============================================================================
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

// =============================================================================
// Wave
// =============================================================================
Vehicle wave() {
    Vehicle v;
    v.type = VehicleType::Wave;

    v.enter = +[](Player& p) {
        // Immediately set hitbox size without lambda heap allocation
        p.size = p.small ? Vec2D(6.0f, 6.0f) : Vec2D(10.0f, 10.0f);
    };

    v.clamp = +[](Player& p) {
        float waveTop = p.grav(p.pos.y + p.grav(p.size.y));
        float waveBottom = p.grav(p.pos.y - p.grav(p.size.y));
        
        // FIXED: Multiplied by p.grav() for upside-down wave inverted controls
        float direction = p.grav(p.input ? 1.0f : -1.0f);
        p.velocity = direction * PHYS_SPEEDS[p.speed] * (p.small ? PHYS_WAVE_SLOPE_SMALL : PHYS_WAVE_SLOPE_NORM);

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

// =============================================================================
// Robot
// =============================================================================
Vehicle robot() {
    Vehicle v;
    v.type = VehicleType::Robot;

    v.enter = +[](Player& p) {
        p.robotBoostTicks = 0;
        p.isRobotBoosting = false;
        VehicleType pv = p.prevPlayer().vehicle.type;
        if (pv == VehicleType::Ship || pv == VehicleType::Wave)
            p.velocity = p.velocity / 4.0f;
        else
            p.velocity = p.velocity / 2.0f;
    };

    v.clamp = +[](Player& p) {
        if (p.velocity < PHYS_ROBOT_MAX_FALL)
            p.velocity = PHYS_ROBOT_MAX_FALL;
        if (p.gravTop(p.innerHitbox()) >= p.gravCeiling())
            p.dead = true;
    };

    v.update = +[](Player& p) {
        p.acceleration = PHYS_ROBOT_GRAVITY[p.speed];
        p.rotation = 0;

        if (p.grounded) {
            p.isRobotBoosting = false;
            p.robotBoostTicks = 0;

            if (p.input) {
                // Initial jump trigger
                p.setVelocity(PHYS_ROBOT_INITIAL_JUMP[p.speed], p.prevPlayer().input);
                p.robotBoostTicks = PHYS_ROBOT_MAX_BOOST_TICKS;
                p.isRobotBoosting = true;
                p.grounded = false;
            } else {
                // Cut velocity if landing
                if (p.velocity > 0.0f) {
                    p.velocity = 0.0f;
                }
            }
            p.buffer = false;
        } else {
            // FIXED: In mid-air, boost force is applied continuously while hold button is down
            if (p.input && p.isRobotBoosting && p.robotBoostTicks > 0) {
                p.velocity += PHYS_ROBOT_BOOST_FORCE;
                p.robotBoostTicks--;
            } else if (!p.input) {
                // Releasing jump cuts the boost permanently until landing
                p.isRobotBoosting = false;
            }
        }
    };

    v.bounds = FLT_MAX;
    return v;
}

// =============================================================================
// Spider
// =============================================================================
Vehicle spider() {
    Vehicle v;
    v.type = VehicleType::Spider;

    v.enter = +[](Player& p) {
        VehicleType pv = p.prevPlayer().vehicle.type;
        if (pv == VehicleType::Ship || pv == VehicleType::Ufo || pv == VehicleType::Wave)
            p.velocity = p.velocity / 2.0f;
    };

    v.clamp = +[](Player& p) {
        p.velocity = std::clamp(p.velocity, -PHYS_SPIDER_MAX_VEL, PHYS_SPIDER_MAX_VEL);
    };

    v.update = +[](Player& p) {
        p.acceleration = PHYS_SPIDER_GRAVITY;
        p.rotation = 0;

        if (p.grounded) {
            p.buffer = false;
        }

        // Spider instant teleportation on click
        if (p.input && !p.prevPlayer().input) {
            p.upsideDown = !p.upsideDown;

            float targetSurfaceY = p.upsideDown ? p.ceiling : p.floor;
            if (p.upsideDown) {
                p.pos.y = targetSurfaceY - p.size.y / 2.0f;
            } else {
                p.pos.y = targetSurfaceY + p.size.y / 2.0f;
            }

            p.velocity = 0.0f;
            p.velocityOverride = true;
            p.grounded = true;
            p.buffer = false;
        }
    };

    v.bounds = 240.0f;
    return v;
}

// =============================================================================
// Swing Copter (GD 2.2)
// =============================================================================
Vehicle swing() {
    Vehicle v;
    v.type = VehicleType::Swing;

    v.enter = +[](Player& p) {
        VehicleType pv = p.prevPlayer().vehicle.type;
        if (pv == VehicleType::Ship || pv == VehicleType::Ufo || pv == VehicleType::Wave)
            p.velocity = p.velocity / 2.0f;
    };

    v.clamp = +[](Player& p) {
        p.velocity = std::clamp(p.velocity, -PHYS_SWING_MAX_VEL, PHYS_SWING_MAX_VEL);
    };

    v.update = +[](Player& p) {
        // Toggles gravity on click
        if (p.input && !p.prevPlayer().input) {
            p.upsideDown = !p.upsideDown;
        }

        p.acceleration = p.grav(PHYS_SWING_GRAVITY);
        rotateFly(p, 0.20f);
    };

    v.bounds = 300.0f;
    return v;
}

// =============================================================================
// Dispatcher
// =============================================================================
Vehicle Vehicle::from(VehicleType v) {
    switch (v) {
        case VehicleType::Cube:   return cube();
        case VehicleType::Ship:   return ship();
        case VehicleType::Ball:   return ball();
        case VehicleType::Ufo:    return ufo();
        case VehicleType::Wave:   return wave();
        case VehicleType::Robot:  return robot();
        case VehicleType::Spider: return spider();
        case VehicleType::Swing:  return swing();
        default:                  return cube();
    }
}