#pragma once
#include <string>
#include <vector>

/**
 * Test levels, one per feature under test.
 *
 * These are level strings in the same format GD stores, so they can be fed
 * straight to the simulator, and also written to a file and imported into the
 * game to check the search against reality.
 */
namespace testlevels {

inline std::string obj(int id, double x, double y) {
	return "1," + std::to_string(id) + ",2," + std::to_string(x) + ",3," + std::to_string(y) + ";";
}

/// Object with an explicit rotation (field 6).
inline std::string rotated(int id, double x, double y, double degrees) {
	return "1," + std::to_string(id) + ",2," + std::to_string(x) + ",3," + std::to_string(y)
		 + ",6," + std::to_string(degrees) + ";";
}

inline std::string ground(int len, double y = -15) {
	std::string s;
	for (int x = 0; x < len; x += 30)
		s += obj(1, x, y);
	return s;
}

inline std::string settings(int speed = 0, bool mini = false, bool upsideDown = false, int vehicle = 0) {
	return "kA4," + std::to_string(speed)
		 + ",kA3," + std::to_string(mini ? 1 : 0)
		 + ",kA11," + std::to_string(upsideDown ? 1 : 0)
		 + ",kA2," + std::to_string(vehicle) + ";";
}

struct TestLevel {
	std::string name;
	std::string description;
	std::string data;
	/// Whether the simulator is expected to model this level exactly.
	bool expectFullySupported = true;
};

/**
 * The phasing regression: a wall built from an object id the simulator has no
 * table entry for.
 *
 * Before the fix, `Object::create` returned nothing for these and the parser
 * dropped them, so the wall did not exist in the search world at all. The
 * search walked straight through and reported a solve; the macro died on the
 * first frame that touched the wall in game.
 */
inline TestLevel phasingWall() {
	std::string lvl = settings() + ground(3000);
	for (int y = 15; y < 200; y += 30)
		lvl += obj(1953, 1000, y);  // a real 2.2 block id
	return {"phasing_wall",
			"Solid wall of an unmodelled 2.2 block id. Must not be walkable.",
			lvl,
			false};
}

/// Rotated spikes at angles that are not multiples of 90 degrees.
inline TestLevel rotatedObjects() {
	std::string lvl = settings() + ground(6000);
	double angles[] = {15, 30, 37, 45, 60, 75};
	for (int i = 0; i < 6; ++i)
		lvl += rotated(8, 700 + i * 700, 15, angles[i]);
	return {"rotated_objects",
			"Spikes at 15/30/37/45/60/75 degrees. Exercises oriented boxes.",
			lvl};
}

/// Every vehicle portal the simulator claims to support.
inline TestLevel vehicleTour() {
	std::string lvl = settings() + ground(14000);
	lvl += obj(13, 800, 105);    // ship
	lvl += obj(12, 3000, 105);   // cube
	lvl += obj(47, 5000, 105);   // ball
	lvl += obj(12, 7000, 105);   // cube
	lvl += obj(111, 9000, 105);  // ufo
	lvl += obj(12, 11000, 105);  // cube
	return {"vehicle_tour",
			"Ship, ball and UFO sections joined by cube portals.",
			lvl};
}

/// Robot portal: declared but not simulated, must be reported.
inline TestLevel robotSection() {
	std::string lvl = settings() + ground(6000);
	lvl += obj(745, 800, 105);
	return {"robot_section",
			"Robot portal. Simulator must report this as unsupported.",
			lvl,
			false};
}

/// Spider portal: declared but not simulated, must be reported.
inline TestLevel spiderSection() {
	std::string lvl = settings() + ground(6000);
	lvl += obj(1331, 800, 105);
	return {"spider_section",
			"Spider portal. Every click teleports, so spam is fatal.",
			lvl,
			false};
}

/// Swing portal: declared but not simulated, must be reported.
inline TestLevel swingSection() {
	std::string lvl = settings() + ground(6000);
	lvl += obj(1933, 800, 105);
	return {"swing_section",
			"Swing portal. Every click flips gravity, so spam is fatal.",
			lvl,
			false};
}

/// Dual mode via level settings.
inline TestLevel dualLevel() {
	std::string lvl = "kA4,0,kA3,0,kA11,0,kA2,0,kA8,1;" + ground(6000);
	for (int x = 600; x < 5000; x += 400)
		lvl += obj(8, x, 15);
	return {"dual_mode",
			"Dual mode enabled in level settings. Both players must be simulated.",
			lvl,
			false};
}

/// Teleport portals.
inline TestLevel teleportPortals() {
	std::string lvl = settings() + ground(6000);
	lvl += obj(747, 1500, 105);
	return {"teleport_portals",
			"Teleport portal. Position changes discontinuously.",
			lvl,
			false};
}

/// Dash orbs.
inline TestLevel dashOrbs() {
	std::string lvl = settings() + ground(6000);
	for (int x = 900; x < 5000; x += 900)
		lvl += obj(1704, x, 60);
	return {"dash_orbs",
			"Dash orbs. Hold-to-dash changes the movement model.",
			lvl,
			false};
}

/// A dense spike run, used to exercise the click rate cap.
inline TestLevel clickRate() {
	std::string lvl = settings() + ground(12000);
	for (int x = 400; x < 11000; x += 150) {
		lvl += obj(8, x, 15);
		lvl += obj(8, x + 30, 15);
	}
	return {"click_rate",
			"Dense spikes that tempt rapid clicking. Macro must stay under 70 CPS.",
			lvl};
}

/// Mixed level of everything currently supported, for an end-to-end solve.
inline TestLevel supportedMix() {
	int len = 12000;
	std::string lvl = settings();
	for (int x = 0; x < len; x += 30) {
		if ((x > 4000 && x < 4600) || (x > 8000 && x < 8400))
			continue;
		lvl += obj(1, x, -15);
	}
	for (int x = 600; x < 3800; x += 350)
		lvl += obj(8, x, 15);
	lvl += obj(36, 4200, 60);
	lvl += obj(36, 4450, 90);
	lvl += obj(35, 5200, 15);
	for (int x = 5600; x < 7800; x += 400)
		lvl += obj(8, x, 15);
	lvl += obj(13, 8050, 120);
	lvl += obj(12, 9200, 105);
	lvl += obj(202, 9600, 45);
	for (int x = 10000; x < 11600; x += 500)
		lvl += obj(8, x, 15);
	return {"supported_mix",
			"Orbs, a pad, a ship section and a speed change. Should solve cleanly.",
			lvl};
}

/// Modifier blocks (D-block, J-block) with conditional state.
inline TestLevel modifierBlocks() {
	std::string lvl = settings() + ground(6000);
	// 1755 is a D-block, 1813 a J-block in 2.2.
	for (int x = 900; x < 5000; x += 900) {
		lvl += obj(1755, x, 15);
		lvl += obj(1813, x + 300, 15);
	}
	return {"modifier_blocks",
			"D-blocks and J-blocks. Behaviour depends on conditional state.",
			lvl,
			false};
}

/// A wall of assorted 2.2 object ids.
inline TestLevel objects22() {
	std::string lvl = settings() + ground(6000);
	int ids[] = {1953, 1954, 1955, 1956, 2000, 2001, 2010, 2064};
	for (int i = 0; i < 8; ++i)
		for (int y = 15; y < 135; y += 30)
			lvl += obj(ids[i], 800 + i * 500, y);
	return {"objects_2_2",
			"Walls built from 2.2 object ids the old tables had no entry for.",
			lvl,
			false};
}

/// Upside-down slopes.
inline TestLevel upsideDownSlopes() {
	std::string lvl = settings() + ground(6000) + ground(6000, 315);
	for (int i = 0; i < 8; ++i) {
		// Slope rotated 180 so it hangs from the ceiling.
		lvl += rotated(289, 700 + i * 600, 285, 180);
	}
	return {"upside_down_slopes",
			"Ceiling-mounted slopes rotated 180 degrees.",
			lvl};
}

inline std::vector<TestLevel> all() {
	return {
		phasingWall(),
		rotatedObjects(),
		vehicleTour(),
		robotSection(),
		spiderSection(),
		swingSection(),
		dualLevel(),
		teleportPortals(),
		dashOrbs(),
		clickRate(),
		supportedMix(),
		modifierBlocks(),
		objects22(),
		upsideDownSlopes(),
	};
}

} // namespace testlevels
