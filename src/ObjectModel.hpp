#pragma once
#include <cstdint>
#include <string>
#include <vector>

/**
 * Behaviour classes the search understands.
 *
 * Deliberately a *behaviour* taxonomy, not an id list. Geometry Dash already
 * tags every object with a `GameObjectType`, and the game itself dispatches
 * collision behaviour from that tag rather than from the object id. Mirroring
 * that means new objects -- including everything added in 2.2 -- classify
 * correctly without anyone maintaining a table.
 *
 * The old simulator hardcoded ~314 ids with a max of 1911 and zero 2.2 ids.
 * Anything outside that list was silently dropped from the collision world,
 * which is the phasing bug. Classification here is derived at runtime from the
 * game's own object data instead.
 */
enum class ObjectBehaviour : uint8_t {
	/// No collision at all: decoration, text, visual-only triggers.
	None = 0,
	/// Solid geometry. Blocks movement, kills on head-on contact.
	Solid,
	/// Kills on contact.
	Hazard,
	/// Breaks when hit, then stops colliding.
	Breakable,
	/// Slope. Needs the dedicated slope solver.
	Slope,
	/// Orb: activates on click while overlapping.
	Orb,
	/// Pad: activates on contact.
	Pad,
	/// Changes vehicle.
	VehiclePortal,
	/// Changes gravity direction.
	GravityPortal,
	/// Changes player size.
	SizePortal,
	/// Changes speed.
	SpeedPortal,
	/// Enters or leaves dual mode.
	DualPortal,
	/// Moves the player discontinuously.
	TeleportPortal,
	/// Dash orb: held contact drives a dash along a rail.
	DashOrb,
	/// Mirror portal. Visual for physics purposes but flips input mapping.
	MirrorPortal,
	/// A trigger that changes level state (move, toggle, spawn, ...).
	Trigger,
	/// Collectible; no physics effect but may gate triggers.
	Collectible,
	/// Modifier block (D-block, J-block and friends) with conditional state.
	Modifier,
	/// Recognised as colliding, but the exact behaviour is not modelled.
	UnsupportedSolid,
};

inline char const* behaviourName(ObjectBehaviour b) {
	switch (b) {
		case ObjectBehaviour::None:             return "none";
		case ObjectBehaviour::Solid:            return "solid";
		case ObjectBehaviour::Hazard:           return "hazard";
		case ObjectBehaviour::Breakable:        return "breakable";
		case ObjectBehaviour::Slope:            return "slope";
		case ObjectBehaviour::Orb:              return "orb";
		case ObjectBehaviour::Pad:              return "pad";
		case ObjectBehaviour::VehiclePortal:    return "vehicle portal";
		case ObjectBehaviour::GravityPortal:    return "gravity portal";
		case ObjectBehaviour::SizePortal:       return "size portal";
		case ObjectBehaviour::SpeedPortal:      return "speed portal";
		case ObjectBehaviour::DualPortal:       return "dual portal";
		case ObjectBehaviour::TeleportPortal:   return "teleport portal";
		case ObjectBehaviour::DashOrb:          return "dash orb";
		case ObjectBehaviour::MirrorPortal:     return "mirror portal";
		case ObjectBehaviour::Trigger:          return "trigger";
		case ObjectBehaviour::Collectible:      return "collectible";
		case ObjectBehaviour::Modifier:         return "modifier block";
		case ObjectBehaviour::UnsupportedSolid: return "unsupported solid";
	}
	return "unknown";
}

/**
 * A single object, described the way the search needs it, captured from the
 * game's live object list.
 *
 * Geometry comes from `GameObject::getObjectRect()` and `getOrientedBox()`,
 * so scale, rotation and any runtime movement are already baked in -- there is
 * no second opinion about how big something is or where it sits.
 */
struct RuntimeObject {
	/// GD object id. Recorded for diagnostics only, never for dispatch.
	int objectId = 0;
	/// Raw GameObjectType value from the game.
	int rawType = 0;
	ObjectBehaviour behaviour = ObjectBehaviour::None;

	/// Centre in level coordinates.
	float x = 0.f;
	float y = 0.f;
	/// Full width and height of the collision box.
	float width = 0.f;
	float height = 0.f;
	/// Degrees, matching the game's convention.
	float rotation = 0.f;

	/// Radius, for objects the game treats as circular (sawblades, orbs).
	float radius = 0.f;

	/// Group ids this object belongs to, for trigger targeting.
	std::vector<int> groups;

	/// True when the object starts disabled or hidden.
	bool startsDisabled = false;
	/// True when the game flags this as a trigger.
	bool isTrigger = false;

	/// Sub-behaviour discriminator, e.g. which vehicle a portal switches to.
	int variant = 0;
};

/**
 * Map a raw `GameObjectType` to a behaviour class.
 *
 * Values mirror `GameObjectType` in Geode's Enums.hpp. Kept as plain integers
 * so this header stays usable from the standalone simulator, which does not
 * link against the game.
 *
 * This is the whole point of the runtime model: one switch over ~45 behaviour
 * tags that the game itself maintains, instead of a table of thousands of ids
 * that goes stale with every content update.
 */
inline ObjectBehaviour behaviourFromGameType(int gameObjectType) {
	switch (gameObjectType) {
		case 0:  return ObjectBehaviour::Solid;            // Solid
		case 2:  return ObjectBehaviour::Hazard;           // Hazard
		case 47: return ObjectBehaviour::Hazard;           // AnimatedHazard

		case 3:  case 4:  return ObjectBehaviour::GravityPortal;
		case 42: return ObjectBehaviour::GravityPortal;    // GravityTogglePortal

		case 5:  case 6:  case 16: case 19:
		case 26: case 27: case 33: case 41:
			return ObjectBehaviour::VehiclePortal;

		case 7:  return ObjectBehaviour::None;             // Decoration

		case 8:  case 9:  case 10: case 34:
		case 44: return ObjectBehaviour::Pad;              // SpiderPad

		case 11: case 12: case 13: case 29:
		case 35: case 36: case 43:
			return ObjectBehaviour::Orb;

		case 37: case 38: return ObjectBehaviour::DashOrb;
		case 46: return ObjectBehaviour::TeleportPortal;   // TeleportOrb
		case 32: return ObjectBehaviour::Orb;              // DropRing

		case 14: case 15: return ObjectBehaviour::MirrorPortal;

		case 17: case 18: return ObjectBehaviour::SizePortal;

		case 20: return ObjectBehaviour::Modifier;
		case 21: return ObjectBehaviour::Breakable;

		case 22: case 30: case 31: return ObjectBehaviour::Collectible;

		case 23: case 24: return ObjectBehaviour::DualPortal;

		case 25: return ObjectBehaviour::Slope;
		case 28: return ObjectBehaviour::TeleportPortal;

		case 39: return ObjectBehaviour::None;             // CollisionObject
		case 40: return ObjectBehaviour::Trigger;          // Special
		case 45: return ObjectBehaviour::None;             // EnterEffectObject

		default:
			// An unrecognised tag is treated as solid rather than ignored.
			// Being wrong this way costs a missed route; being wrong the other
			// way costs a macro that phases through geometry and kills you.
			return ObjectBehaviour::UnsupportedSolid;
	}
}

/// Whether a behaviour participates in collision at all.
inline bool behaviourCollides(ObjectBehaviour b) {
	switch (b) {
		case ObjectBehaviour::None:
		case ObjectBehaviour::Trigger:
		case ObjectBehaviour::Collectible:
			return false;
		default:
			return true;
	}
}

/// Whether the search can model this behaviour exactly today.
inline bool behaviourSupported(ObjectBehaviour b) {
	switch (b) {
		case ObjectBehaviour::TeleportPortal:
		case ObjectBehaviour::DashOrb:
		case ObjectBehaviour::DualPortal:
		case ObjectBehaviour::Modifier:
		case ObjectBehaviour::UnsupportedSolid:
			return false;
		default:
			return true;
	}
}
