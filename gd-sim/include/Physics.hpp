#pragma once

/**
 * Physics constants for Geometry Dash simulation.
 * All values are calibrated to match real Geometry Dash behavior.
 * Using float consistently to match GD's internal 32-bit precision.
 */

// Player X velocity per speed (units per second)
constexpr float PHYS_SPEEDS[5] = {
    251.16007f,
    311.58009f,
    387.42014f,
    468.00014f,
    576.00020f
};

// Rotation speed multipliers
constexpr float PHYS_SPEED_MULTS[5] = {
    0.7f,
    0.9f,
    1.1f,
    1.3f,
    1.6f
};

// Velocity thresholds for ship/ufo acceleration switching
constexpr float PHYS_VELOCITY_THRESHOLDS[5] = {
    101.541492f,
    103.485495f,
    103.377492f,
    103.809492f,
    103.809492f
};

// --- Cube ---
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

// --- Ship ---
// Ship acceleration when input is held (rising)
constexpr float PHYS_SHIP_ACCEL_RISE_HIGH[5] = { 1643.5872f, 1643.5872f, 1643.5872f, 1643.5872f, 1643.5872f }; // small
constexpr float PHYS_SHIP_ACCEL_RISE_LOW[5]  = { 1397.0491f, 1397.0491f, 1397.0491f, 1397.0491f, 1397.0491f }; // normal
constexpr float PHYS_SHIP_ACCEL_RISE_HIGH_SMALL = 1643.5872f;
constexpr float PHYS_SHIP_ACCEL_RISE_LOW_SMALL  = 1314.86976f;
constexpr float PHYS_SHIP_ACCEL_RISE_HIGH_NORM  = 1397.0491f;
constexpr float PHYS_SHIP_ACCEL_RISE_LOW_NORM   = 1117.64328f;

// Ship acceleration when input is released (falling)
constexpr float PHYS_SHIP_ACCEL_FALL_HIGH_SMALL = -1577.85408f;
constexpr float PHYS_SHIP_ACCEL_FALL_LOW_SMALL  = -1051.8984f;
constexpr float PHYS_SHIP_ACCEL_FALL_HIGH_NORM  = -1341.1719f;
constexpr float PHYS_SHIP_ACCEL_FALL_LOW_NORM   = -894.11464f;

constexpr float PHYS_SHIP_MAX_RISE_NORM = 432.0f;
constexpr float PHYS_SHIP_MAX_FALL_NORM = -345.6f;
constexpr float PHYS_SHIP_MAX_RISE_SMALL = 508.248f;
constexpr float PHYS_SHIP_MAX_FALL_SMALL = -406.566f;

// --- Ball ---
constexpr float PHYS_BALL_GRAVITY = -1676.46672f;
constexpr float PHYS_BALL_JUMP[5] = {
    -172.044007f,
    -181.11601f,
    -185.00401f,
    -181.92601f,
    -181.92601f
};
constexpr float PHYS_BALL_MAX_VEL = 810.0f;

// --- UFO ---
constexpr float PHYS_UFO_JUMP_NORM  = 371.034f;
constexpr float PHYS_UFO_JUMP_SMALL = 358.992f;

constexpr float PHYS_UFO_GRAV_HIGH_SMALL = -1969.92f;
constexpr float PHYS_UFO_GRAV_LOW_SMALL  = -1308.96f;
constexpr float PHYS_UFO_GRAV_HIGH_NORM  = -1671.84f;
constexpr float PHYS_UFO_GRAV_LOW_NORM   = -1114.56f;

constexpr float PHYS_UFO_MAX_RISE_NORM = 432.0f;
constexpr float PHYS_UFO_MAX_FALL_NORM = -345.6f;
constexpr float PHYS_UFO_MAX_RISE_SMALL = 508.24f;
constexpr float PHYS_UFO_MAX_FALL_SMALL = -406.56f;

// --- General ---
constexpr float PHYS_FPS = 240.0f;
constexpr float PHYS_DT = 1.0f / PHYS_FPS;
constexpr float PHYS_VEL_UNIT = 54.0f; // GD stores velocity as 1/54 units
constexpr float PHYS_Y_OFFSET = 105.0f; // GD's Y position offset
constexpr int   PHYS_COYOTE_FRAMES_NORMAL = 10;
constexpr int   PHYS_COYOTE_FRAMES_BALL = 16;
constexpr float PHYS_DEATH_Y_HIGH = 1476.3f;
constexpr float PHYS_DEATH_Y_LOW = 0.0f;
