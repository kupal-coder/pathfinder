#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "search.hpp"

/**
 * In-game verification of a candidate macro.
 *
 * The search runs against gd-sim, which is a *reimplementation* of Geometry
 * Dash's physics. However careful it is, it is a second opinion, and the only
 * opinion that decides whether a macro dies is the game's own. So before a
 * macro is presented as solved, it is replayed through the real PlayLayer:
 * real objects, real collision callbacks, real death conditions.
 *
 * This is what closes the loop on the phasing bug. Even with unknown objects
 * now modelled as solid, a guessed hitbox is still a guess. Verification is not
 * "did our simulation agree with itself" but "did the game let us through".
 *
 * On a mismatch -- the simulator said alive, the game killed us -- the object
 * responsible is captured from `PlayLayer::destroyPlayer(player, object)`,
 * which hands us the exact GameObject, and the path is rejected.
 */

/// Why a verification run ended.
enum class VerifyOutcome : uint8_t {
	/// Reached the end of the level alive.
	Completed,
	/// Died. See `killerObjectId` / position.
	Died,
	/// Ran past the frame budget without finishing.
	TimedOut,
	/// Could not run at all (no PlayLayer, level failed to load).
	Unavailable,
};

inline char const* verifyOutcomeName(VerifyOutcome o) {
	switch (o) {
		case VerifyOutcome::Completed:   return "completed";
		case VerifyOutcome::Died:        return "died";
		case VerifyOutcome::TimedOut:    return "timed out";
		case VerifyOutcome::Unavailable: return "unavailable";
	}
	return "unknown";
}

struct VerifyReport {
	VerifyOutcome outcome = VerifyOutcome::Unavailable;

	/// Frame the run ended on.
	int frame = 0;
	/// Player position when the run ended.
	float x = 0.f;
	float y = 0.f;

	/// The object that killed the player, when known.
	int killerObjectId = 0;
	float killerX = 0.f;
	float killerY = 0.f;
	/// Raw GameObjectType of the killer, for diagnosing behaviour mapping.
	int killerType = -1;

	bool ok() const { return outcome == VerifyOutcome::Completed; }

	/**
	 * One-line diagnostic naming the colliding object and where it was.
	 *
	 * This is the message that turns "the macro just dies" into something
	 * actionable: it says which object the search failed to model and where to
	 * look for it in the editor.
	 */
	std::string describe() const {
		switch (outcome) {
			case VerifyOutcome::Completed:
				return "verified: completed in game";
			case VerifyOutcome::Died: {
				std::string s = "sim/real mismatch at frame " + std::to_string(frame)
							  + " (x " + std::to_string(static_cast<int>(x))
							  + ", y " + std::to_string(static_cast<int>(y)) + ")";
				if (killerObjectId) {
					s += " killed by object id " + std::to_string(killerObjectId);
					if (killerType >= 0)
						s += " (type " + std::to_string(killerType) + ")";
					s += " at (" + std::to_string(static_cast<int>(killerX))
					   + ", " + std::to_string(static_cast<int>(killerY)) + ")";
				}
				return s;
			}
			case VerifyOutcome::TimedOut:
				return "verification ran past its frame budget at x "
					 + std::to_string(static_cast<int>(x));
			case VerifyOutcome::Unavailable:
				return "verification unavailable";
		}
		return "";
	}
};

/**
 * Replay `tape` through the real game and report what happened.
 *
 * Must be called on the main thread: it drives PlayLayer, which is not
 * thread-safe. The implementation lives in Verifier.cpp, which is the only
 * place that touches game bindings, so the search itself stays portable and
 * testable without the game.
 *
 * `maxFrames` bounds the run so a macro that loops cannot hang the game.
 */
VerifyReport verifyInGame(pf::InputTape const& tape, int maxFrames = 200000);

/// Whether in-game verification can run right now (a level is loaded).
bool verificationAvailable();
