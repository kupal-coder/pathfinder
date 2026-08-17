/**
 * Tests for runtime object classification.
 *
 * The point of this model is that behaviour is derived from the game's own
 * `GameObjectType` tag rather than from a table of object ids. These tests pin
 * that mapping down, because getting it wrong is exactly the failure that
 * produced the phasing bug: an object classified as "nothing" disappears from
 * the collision world and the search plans a route through it.
 *
 * The critical property, asserted last, is that an *unrecognised* tag is
 * treated as solid rather than ignored.
 */
#include "../../src/ObjectModel.hpp"

#include <iostream>
#include <set>
#include <string>

static int g_failures = 0;

static void check(bool cond, std::string const& what) {
	std::cout << (cond ? "  ok    " : "  FAIL  ") << what << "\n";
	if (!cond)
		++g_failures;
}

static void expectBehaviour(int type, ObjectBehaviour want, std::string const& label) {
	auto got = behaviourFromGameType(type);
	check(got == want,
		  label + " (type " + std::to_string(type) + ") -> " + behaviourName(want)
			  + (got == want ? "" : std::string(", got ") + behaviourName(got)));
}

int main() {
	std::cout << "-- behaviour derived from GameObjectType --\n";

	expectBehaviour(0,  ObjectBehaviour::Solid,          "Solid");
	expectBehaviour(2,  ObjectBehaviour::Hazard,         "Hazard");
	expectBehaviour(47, ObjectBehaviour::Hazard,         "AnimatedHazard");
	expectBehaviour(7,  ObjectBehaviour::None,           "Decoration");
	expectBehaviour(21, ObjectBehaviour::Breakable,      "Breakable");
	expectBehaviour(25, ObjectBehaviour::Slope,          "Slope");

	std::cout << "\n-- portals --\n";
	expectBehaviour(3,  ObjectBehaviour::GravityPortal,  "InverseGravityPortal");
	expectBehaviour(4,  ObjectBehaviour::GravityPortal,  "NormalGravityPortal");
	expectBehaviour(42, ObjectBehaviour::GravityPortal,  "GravityTogglePortal");
	expectBehaviour(17, ObjectBehaviour::SizePortal,     "RegularSizePortal");
	expectBehaviour(18, ObjectBehaviour::SizePortal,     "MiniSizePortal");
	expectBehaviour(23, ObjectBehaviour::DualPortal,     "DualPortal");
	expectBehaviour(24, ObjectBehaviour::DualPortal,     "SoloPortal");
	expectBehaviour(28, ObjectBehaviour::TeleportPortal, "TeleportPortal");
	expectBehaviour(14, ObjectBehaviour::MirrorPortal,   "InverseMirrorPortal");
	expectBehaviour(15, ObjectBehaviour::MirrorPortal,   "NormalMirrorPortal");

	std::cout << "\n-- every vehicle portal classifies as one --\n";
	for (int type : {5, 6, 16, 19, 26, 27, 33, 41})
		expectBehaviour(type, ObjectBehaviour::VehiclePortal, "vehicle portal");

	std::cout << "\n-- orbs, pads, dash orbs --\n";
	for (int type : {11, 12, 13, 29, 35, 36, 43})
		expectBehaviour(type, ObjectBehaviour::Orb, "orb");
	for (int type : {8, 9, 10, 34, 44})
		expectBehaviour(type, ObjectBehaviour::Pad, "pad");
	expectBehaviour(37, ObjectBehaviour::DashOrb,        "DashRing");
	expectBehaviour(38, ObjectBehaviour::DashOrb,        "GravityDashRing");
	expectBehaviour(46, ObjectBehaviour::TeleportPortal, "TeleportOrb");

	std::cout << "\n-- modifiers and collectibles --\n";
	expectBehaviour(20, ObjectBehaviour::Modifier,     "Modifier (D-block/J-block)");
	expectBehaviour(22, ObjectBehaviour::Collectible,  "SecretCoin");
	expectBehaviour(30, ObjectBehaviour::Collectible,  "Collectible");
	expectBehaviour(31, ObjectBehaviour::Collectible,  "UserCoin");

	std::cout << "\n-- collision participation --\n";
	check(behaviourCollides(ObjectBehaviour::Solid),        "solids collide");
	check(behaviourCollides(ObjectBehaviour::Hazard),       "hazards collide");
	check(behaviourCollides(ObjectBehaviour::Orb),          "orbs collide");
	check(!behaviourCollides(ObjectBehaviour::None),        "decoration does not collide");
	check(!behaviourCollides(ObjectBehaviour::Trigger),     "triggers do not collide");
	check(!behaviourCollides(ObjectBehaviour::Collectible), "collectibles do not collide");

	std::cout << "\n-- unknown tags fail safe --\n";

	// This is the property that prevents the phasing bug from coming back. An
	// object the model does not recognise must still block, and must still be
	// reported as not exactly modelled.
	for (int type : {48, 60, 99, 255, 1000, -1}) {
		auto b = behaviourFromGameType(type);
		check(b == ObjectBehaviour::UnsupportedSolid,
			  "unrecognised type " + std::to_string(type) + " is UnsupportedSolid");
		check(behaviourCollides(b),
			  "unrecognised type " + std::to_string(type) + " still collides");
		check(!behaviourSupported(b),
			  "unrecognised type " + std::to_string(type) + " is reported unsupported");
	}

	std::cout << "\n-- known-unsupported behaviours are flagged --\n";
	for (auto b : {ObjectBehaviour::TeleportPortal, ObjectBehaviour::DashOrb,
				   ObjectBehaviour::DualPortal, ObjectBehaviour::Modifier})
		check(!behaviourSupported(b),
			  std::string(behaviourName(b)) + " is reported as not yet modelled");

	for (auto b : {ObjectBehaviour::Solid, ObjectBehaviour::Hazard, ObjectBehaviour::Orb,
				   ObjectBehaviour::Pad, ObjectBehaviour::Slope, ObjectBehaviour::VehiclePortal})
		check(behaviourSupported(b),
			  std::string(behaviourName(b)) + " is modelled");

	std::cout << "\n-- no GameObjectType silently vanishes --\n";

	// Sweep every tag the game defines. None may map to None unless it is
	// genuinely non-colliding, because a wrong None is an invisible object.
	std::set<int> legitimatelyInvisible = {7, 39, 45};
	int vanished = 0;
	for (int type = 0; type <= 47; ++type) {
		auto b = behaviourFromGameType(type);
		if (b == ObjectBehaviour::None && !legitimatelyInvisible.count(type)) {
			std::cout << "      type " << type << " maps to None unexpectedly\n";
			++vanished;
		}
	}
	check(vanished == 0, "no colliding GameObjectType maps to None");

	std::cout << "\n" << g_failures << " failure(s)\n";
	return g_failures == 0 ? 0 : 1;
}
