#pragma once
#include <Object.hpp>
#include <Player.hpp>
#include <vector>


/**
 * In order to use the simulator, you must create a Level. The Level class is
 * the root class of everything else, containing both objects and player states,
 * as well as the main update function. See Level.cpp for implementation info.
 */
class Level {
	/// Called by constructor, applies level settings to the initial player state
	void initLevelSettings(std::string const& lvlSettings, Player& player);
	/// Post-parse: link teleport portal pairs by group ID
	void linkTeleportPortals();
 public:
 	/**
 	 * All player states are stored, including previous states. This way, Pathfinder
 	 * is able to seamlessly rewind when searching for solutions.
 	 */
	std::vector<Player> gameStates;

	size_t objectCount = 0;

	/// Sections are used just like real GD. See Object.hpp for more info on ObjectContainer.
	std::vector<std::vector<ObjectContainer>> sections;

	float length = 0.0;

 	static constexpr uint32_t sectionSize = 100;
 	bool debug = false;

 	Level(std::string const& lvlString);

 	/// The main update function. Every frame is associated with a press/release state.
 	Player& runFrame(bool pressed, float dt = 1/240.);

 	/// Go back to a certain frame. Used in Pathfinder.
 	void rollback(int frame);

 	int currentFrame() const;
 	Player const& getState(int frame) const;
 	Player& latestState();
};
