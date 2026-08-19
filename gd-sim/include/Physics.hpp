#pragma once

/**
 * Physics constants for Geometry Dash simulation.
 * Calibrated to match real Geometry Dash (2.2 / Cocos2d-x) 32-bit float precision.
 */

// =============================================================================
// General Engine & Speed Settings
// =============================================================================

constexpr float PHYS_FPS        = 240.0f;
constexpr float PHYS_DT         = 1.0f / PHYS_FPS;
constexpr float PHYS_VEL_UNIT   = 54.0f;  // GD internal velocity unit (1/54)
constexpr float PHYS_Y_OFFSET   = 105.0f; // GD base floor Y offset

constexpr float PHYS_DEATH_Y_HIGH = 1476.3f;
constexpr float PHYS_DEATH_Y_LOW  = 0.0f;

constexpr int PHYS_COYOTE_FRAMES_NORMAL = 10;
constexpr int PHYS_COYOTE_FRAMES_BALL   = 16;

// Player X horizontal speed per portal [0.5x, 1.0x, 2.0x, 3.0x, 4.0x] (units/sec)
constexpr float PHYS_SPEEDS[5] = {
    251.16007f, // 0.5x (Slow)
    311.58009f, // 1.0x (Normal)
    387.42014f, // 2.0x (Fast)
    468.00014f, // 3.0x (Faster)
    576.00020f  // 4.0x (Fastest)
};

// Rotation speed multipliers per speed portal
constexpr float PHYS_SPEED_MULTS[5] = {
    0.7f,
    0.9f,
    1.1f,
    1.3f,
    1.6f
};

// Velocity thresholds for piecewise ship/ufo acceleration
constexpr float PHYS_VELOCITY_THRESHOLDS[5] = {
    101.541492f,
    103.485495f,
    103.377492f,
    103.809492f,
    103.809492f
};

// =============================================================================
// Cube
// =============================================================================

constexpr float PHYS_CUBE_GRAVITY[5] = {
    -2747.52f,
    -2794.1082f,
    -2786.4f,
    -2799.36f,
    -2799.36f
};

constexpr float PHYS_CUBE_JUMP[5] = {
    573.481728f,
    603.721717f,
    616.681728f,
    606.421728f,
    606.421728f
};

constexpr float PHYS_CUBE_MAX_FALL = -810.0f;

// =============================================================================
// Ship
// =============================================================================

// Normal Ship (Input Held / Rising)
constexpr float PHYS_SHIP_ACCEL_RISE_HIGH_NORM = 1397.0491f;
constexpr float PHYS_SHIP_ACCEL_RISE_LOW_NORM  = 1117.64328f;

// Normal Ship (Input Released / Falling)
constexpr float PHYS_SHIP_ACCEL_FALL_HIGH_NORM = -1341.1719f;
constexpr float PHYS_SHIP_ACCEL_FALL_LOW_NORM  = -894.11464f;

// Mini Ship (Input Held / Rising)
constexpr float PHYS_SHIP_ACCEL_RISE_HIGH_SMALL = 1643.5872f;
constexpr float PHYS_SHIP_ACCEL_RISE_LOW_SMALL  = 1314.86976f;

// Mini Ship (Input Released / Falling)
constexpr float PHYS_SHIP_ACCEL_FALL_HIGH_SMALL = -1577.85408f;
constexpr float PHYS_SHIP_ACCEL_FALL_LOW_SMALL  = -1051.8984f;

// Terminal Velocities
constexpr float PHYS_SHIP_MAX_RISE_NORM  = 432.0f;
constexpr float PHYS_SHIP_MAX_FALL_NORM  = -345.6f;
constexpr float PHYS_SHIP_MAX_RISE_SMALL = 508.248f;
constexpr float PHYS_SHIP_MAX_FALL_SMALL = -406.566f;

// =============================================================================
// Ball
// =============================================================================

constexpr float PHYS_BALL_GRAVITY = -1676.46672f;
constexpr float PHYS_BALL_JUMP[5] = {
    172.044007f, // Magnitude applied when gravity flips
    181.116010f,
    185.004010f,
    181.926010f,
    181.926010f
};
constexpr float PHYS_BALL_MAX_VEL = 810.0f;

// =============================================================================
// UFO
// =============================================================================

constexpr float PHYS_UFO_JUMP_NORM  = 371.034f;
constexpr float PHYS_UFO_JUMP_SMALL = 358.992f;

constexpr float PHYS_UFO_GRAV_HIGH_NORM  = -1671.84f;
constexpr float PHYS_UFO_GRAV_LOW_NORM   = -1114.56f;
constexpr float PHYS_UFO_GRAV_HIGH_SMALL = -1969.92f;
constexpr float PHYS_UFO_GRAV_LOW_SMALL  = -1308.96f;

constexpr float PHYS_UFO_MAX_RISE_NORM  = 432.0f;
constexpr float PHYS_UFO_MAX_FALL_NORM  = -345.6f;
constexpr float PHYS_UFO_MAX_RISE_SMALL = 508.24f;
constexpr float PHYS_UFO_MAX_FALL_SMALL = -406.56f;

// =============================================================================
// Robot
// =============================================================================

constexpr float PHYS_ROBOT_GRAVITY[5] = {
    -2747.52f,
    -2794.1082f,
    -2786.4f,
    -2799.36f,
    -2799.36f
};

constexpr float PHYS_ROBOT_INITIAL_JUMP[5] = {
    573.481728f,
    603.721717f,
    616.681728f,
    606.421728f,
    606.421728f
};

constexpr float PHYS_ROBOT_MAX_FALL = -810.0f;

// FIXED: In GD, Robot does not have multi-jump. It uses a hold-boost timer (~0.35s at 240Hz).
constexpr float PHYS_ROBOT_MAX_BOOST_TIME  = 0.354f; // Max boost duration in seconds
constexpr int   PHYS_ROBOT_MAX_BOOST_TICKS = 85;     // Boost duration in 240Hz ticks
constexpr float PHYS_ROBOT_BOOST_FORCE     = 43.2f;  // Acceleration added per tick while held

// =============================================================================
// Spider
// =============================================================================

constexpr float PHYS_SPIDER_GRAVITY = -1676.46672f;
constexpr float PHYS_SPIDER_MAX_VEL = 810.0f;

// =============================================================================
// Wave
// =============================================================================

// Y velocity equals X velocity times the slope factor
constexpr float PHYS_WAVE_SLOPE_NORM  = 1.0f; // 45 degrees
constexpr float PHYS_WAVE_SLOPE_SMALL = 2.0f; // ~63.4 degrees

// =============================================================================
// Swing Copter (GD 2.2)
// =============================================================================

constexpr float PHYS_SWING_GRAVITY   = -1080.0f;
constexpr float PHYS_SWING_MAX_VEL   = 432.0f;