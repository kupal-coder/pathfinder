#pragma once
#include <Object.hpp>
#include <Player.hpp>
#include <memory>
#include <vector>
#include <unordered_map>

/// Static level geometry: sections of objects built once at parse time
/// (including teleport-portal links). Never mutated afterwards, so every
/// Level copy/branch shares a single instance instead of deep-copying all
/// objects on each copy. Per-run dynamic state lives in Level::gameStates.
using SectionList = std::vector<std::vector<ObjectContainer>>;


/**
 * In order to use the simulator, you must create a Level. The Level class is
 * the root class of everything else, containing both objects and player states,
 * as well as the main update function. See Level.cpp for implementation info.
 */
class Level {
	/// Called by constructor, applies level settings to the initial player state
	void initLevelSettings(std::string const& lvlSettings, Player& player);
	/// Post-parse: link teleport portal pairs by group ID (build time only).
	static void linkTeleportPortals(SectionList& sections);
	/// Rebind copied player state to this Level instance. Geometry is shared
	/// and never mutated after parse, so only gameStates need rebinding.
	void rebindCopiedState();
 public:
 	/**
 	 * All player states are stored, including previous states. This way, Pathfinder
 	 * is able to seamlessly rewind when searching for solutions.
 	 */
	std::vector<Player> gameStates;

	size_t objectCount = 0;

	/// Coverage bookkeeping: object ids seen at parse time that have no sim
	/// mapping (decorations, triggers, not-yet-implemented blocks...).
	/// Read-only after parse; diagnostics only, never used for solving.
	/// Copy/move ops below must keep it in sync with the copied state.
	std::unordered_map<int, int> ignoredObjectIds;

	/// Shared static geometry (see SectionList). Sections are used just like
	/// real GD. See Object.hpp for more info on ObjectContainer.
	std::shared_ptr<const SectionList> geom;

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
 	Player& latestState();
};
