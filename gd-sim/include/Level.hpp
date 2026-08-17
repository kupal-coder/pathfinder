#pragma once
#include <Object.hpp>
#include <Player.hpp>
#include <StateHistory.hpp>
#include <vector>


/**
 * In order to use the simulator, you must create a Level. The Level class is
 * the root class of everything else, containing both objects and player states,
 * as well as the main update function. See Level.cpp for implementation info.
 */
class Level {
	/// Called by constructor, applies level settings to the initial player state
	void initLevelSettings(std::string const& lvlSettings, Player& player);
 public:
 	/**
 	 * Player state history.
 	 *
 	 * Bounded to a small window by default so that forking a search candidate
 	 * is cheap -- see StateHistory.hpp for the measurements that motivated it.
 	 * Tools that need the whole trace (the debug overlay) can opt into
 	 * unbounded mode with `setFullHistory(true)`.
 	 */
	StateHistory history;

	size_t objectCount = 0;

	/**
	 * Objects in a section, pre-partitioned by collision priority.
	 *
	 * GD processes generic objects first, then blocks in descending order, then
	 * hazards. That partition used to be recomputed every frame for every
	 * object in three sections; it is static, so it is done once at parse time.
	 */
	struct Section {
		std::vector<ObjectContainer> generic;  ///< prio 0
		std::vector<ObjectContainer> blocks;   ///< prio 1
		std::vector<ObjectContainer> hazards;  ///< prio 2

		bool empty() const { return generic.empty() && blocks.empty() && hazards.empty(); }
	};

	/// Sections are used just like real GD. See Object.hpp for more info on ObjectContainer.
	std::vector<Section> sections;

	float length = 0.0;

 	static constexpr uint32_t sectionSize = 100;
 	bool debug = false;

 	Level(std::string const& lvlString);

 	/**
 	 * Retain every frame instead of a sliding window.
 	 *
 	 * Costs ~368 bytes per frame and makes forking expensive, so this is for
 	 * inspection tools only -- never for search. Resets the run.
 	 */
 	void setFullHistory(bool full);

 	/// The main update function. Every frame is associated with a press/release state.
 	Player& runFrame(bool pressed, float dt = 1/240.);

 	/// Go back to a certain frame. Used in the pathfinder.
 	void rollback(int frame);

 	int currentFrame() const;
 	Player const& getState(int frame) const;
 	Player& latestState();

 	/// Oldest frame still available to `getState`.
 	int oldestFrame() const { return history.oldestFrame(); }

 	/**
 	 * Slopes touched this frame, used by Block collision.
 	 *
 	 * Lives on the Level rather than the Player because it is within-frame
 	 * scratch, not state: keeping it out of Player is part of what makes Player
 	 * trivially copyable.
 	 */
 	std::vector<Slope const*> const& potentialSlopes() const { return m_potentialSlopes; }
 	void addPotentialSlope(Slope const* s) { m_potentialSlopes.push_back(s); }
 	void clearPotentialSlopes() { m_potentialSlopes.clear(); }

 	/// Snapshot/restore for cheap search branching.
 	struct Checkpoint {
 		StateHistory::Checkpoint history;
 	};
 	Checkpoint checkpoint() const;
 	void restore(Checkpoint const& cp);

 private:
 	std::vector<Slope const*> m_potentialSlopes;
};
