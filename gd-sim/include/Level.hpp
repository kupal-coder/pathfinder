#pragma once
#include <Object.hpp>
#include <Player.hpp>
#include <optional>
#include <unordered_map>
#include <vector>


/**
 * In order to use the simulator, you must create a Level. The Level class is
 * the root class of everything else, containing both objects and player states,
 * as well as the main update function. See Level.cpp for implementation info.
 */
class Level {
	/// Called by constructor, applies level settings to the initial player state
	void initLevelSettings(std::string const& lvlSettings, Player& player);

	/// Re-points every stored state at this level. Player holds a `Level*`, so a
	/// copied Level would otherwise have states pointing at the *original* level;
	/// prevPlayer() would then read another level's (possibly rolled back) history.
	void rebind();
 public:
 	/**
 	 * All player states are stored, including previous states. This way, Pathfinder
 	 * is able to seamlessly rewind when searching for solutions.
 	 */
	std::vector<Player> gameStates;

	/**
	 * Second player states, only populated once a dual portal has been hit.
	 * Indexed by `frame - dualStartFrame`. See `dual` below.
	 */
	std::vector<Player> gameStates2;

	/// Frame the dual portal was hit on, or 0 when the level is not dual.
	int dualStartFrame = 0;

	/// Whether a second player is currently being simulated.
	bool dual = false;

	size_t objectCount = 0;

	/// Sections are used just like real GD. See Object.hpp for more info on ObjectContainer.
	std::vector<std::vector<ObjectContainer>> sections;

	/**
	 * Object IDs that appear in the level string but have no simulator
	 * implementation, mapped to how many times they occur. Anything in here is
	 * invisible to the simulation, so a macro may desync on it.
	 */
	std::unordered_map<int, int> unknownObjects;

	float length = 0.0;

 	static constexpr uint32_t sectionSize = 100;
 	bool debug = false;

 	Level(std::string const& lvlString);

	Level(Level const& other);
	Level& operator=(Level const& other);
	Level(Level&& other) noexcept;
	Level& operator=(Level&& other) noexcept;

 	/// The main update function. Every frame is associated with a press/release state.
 	Player& runFrame(bool pressed, float dt = 1/240.);

 	/// Go back to a certain frame. Used in Pathfinder.
 	void rollback(int frame);

 	int currentFrame() const;
 	Player const& getState(int frame) const;
 	Player const& getState(int frame, bool second) const;
 	Player& latestState();
	Player const& latestState() const;

	/// Both players are simulated in dual; the run ends when either one dies.
	bool anyDead() const;

	/**
	 * Nearest surface directly overhead (relative to the player's gravity), used
	 * by spider teleports. Returns the y the player centre should end up at, or
	 * nullopt when nothing is in the way and the vehicle ceiling should be used.
	 */
	std::optional<float> spiderTarget(Player const& p) const;

 private:
	/// Steps a single player through preCollision -> collisions -> postCollision.
	void stepPlayer(Player& p, bool pressed, float dt);
};
