#pragma once

#include <functional>

enum class VehicleType {
	Cube,
	Ship,
	Ball,
	Ufo,
	Wave,
	/**
	 * Not yet simulated. Declared so that the rest of the codebase -- the click
	 * rate limiter, the state key, the UI -- can reason about them, and so that
	 * a level containing one is detected and reported rather than silently
	 * simulated as a cube, which would produce a confidently wrong macro.
	 */
	Robot,
	Spider,
	Swing,
};

/// Vehicles the simulator can actually model.
inline bool isVehicleSupported(VehicleType v) {
	switch (v) {
		case VehicleType::Robot:
		case VehicleType::Spider:
		case VehicleType::Swing:
			return false;
		default:
			return true;
	}
}

inline char const* vehicleName(VehicleType v) {
	switch (v) {
		case VehicleType::Cube:   return "cube";
		case VehicleType::Ship:   return "ship";
		case VehicleType::Ball:   return "ball";
		case VehicleType::Ufo:    return "UFO";
		case VehicleType::Wave:   return "wave";
		case VehicleType::Robot:  return "robot";
		case VehicleType::Spider: return "spider";
		case VehicleType::Swing:  return "swing";
	}
	return "unknown";
};

struct Player;
struct Object;

/// NOT the same as vehicle portal. This class exists to hold vehicle-specific logic.
struct Vehicle {
	VehicleType type;

	/// When the vehicle is changed into this one.
	std::function<void(Player&)> enter;

	/// Ran after everything else, used mainly for vehicles that have ceilings
	std::function<void(Player&)> clamp;

	/// Vehicle-specific movement, done after collisions
	std::function<void(Player&)> update;

	/// How far away the floor and ceiling are from each other, relative to portal
	float bounds;

	static Vehicle from(VehicleType v);
};
