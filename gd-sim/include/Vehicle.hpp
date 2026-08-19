#pragma once

#include <cstdint>

// Forward declaration
struct Player;

// =============================================================================
// Vehicle Gamemodes
// =============================================================================
enum class VehicleType : uint8_t {
    Cube   = 0,
    Ship   = 1,
    Ball   = 2,
    Ufo    = 3,
    Wave   = 4,
    Robot  = 5,
    Spider = 6,
    Swing  = 7  // Added: GD 2.2 Swing Copter
};

// Function pointer signature for vehicle lifecycle routines
using VehicleAction = void (*)(Player&);

/**
 * Holds vehicle-specific physics and lifecycle handlers.
 * Optimized with raw function pointers for zero-allocation, fast per-frame copying.
 */
struct Vehicle {
    VehicleType type = VehicleType::Cube;

    /// Called when the player transforms into this vehicle
    VehicleAction enter  = nullptr;

    /// Runs after physics, primarily for vehicles restricted by ceilings (e.g. Ship, UFO)
    VehicleAction clamp  = nullptr;

    /// Vehicle-specific movement/input calculations executed during physics step
    VehicleAction update = nullptr;

    /// Vertical distance between floor and ceiling boundaries
    float bounds         = 0.0f;

    /// Factory method to construct the pre-configured vehicle definition
    static Vehicle from(VehicleType v);

    // --- Comparison helpers for clean syntax ---
    constexpr bool operator==(VehicleType other) const { return type == other; }
    constexpr bool operator!=(VehicleType other) const { return type != other; }
    constexpr bool operator==(Vehicle const& other) const { return type == other.type; }
    constexpr bool operator!=(Vehicle const& other) const { return type != other.type; }
};