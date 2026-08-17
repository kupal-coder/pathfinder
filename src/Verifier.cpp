#include "Verifier.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;

/**
 * Capture of the object that killed the player.
 *
 * PlayLayer::destroyPlayer receives the exact GameObject responsible, which is
 * the one piece of information that makes a sim/real mismatch diagnosable. It
 * is recorded into this slot by the hook below and read straight after the
 * stepping loop notices the death.
 */
namespace {

struct KillerSlot {
	bool valid = false;
	int objectId = 0;
	int objectType = -1;
	float x = 0.f;
	float y = 0.f;

	void clear() { *this = KillerSlot{}; }
};

KillerSlot g_killer;

/// Set while a verification run is stepping, so the hook only records then.
bool g_verifying = false;

} // namespace

class $modify(VerifierPlayLayer, PlayLayer) {
	// State is global rather than per-instance: only one verification runs at
	// a time and it is driven synchronously from the main thread.
	void destroyPlayer(PlayerObject* player, GameObject* object) {
		// Record the killer before the game tears the state down. Only during a
		// verification run -- normal gameplay deaths are none of our business.
		if (g_verifying && !g_killer.valid) {
			g_killer.valid = true;
			if (object) {
				g_killer.objectId = object->m_objectID;
				g_killer.objectType = static_cast<int>(object->m_objectType);
				auto pos = object->getPosition();
				g_killer.x = pos.x;
				g_killer.y = pos.y;
			}
		}

		PlayLayer::destroyPlayer(player, object);
	}
};

bool verificationAvailable() {
	return PlayLayer::get() != nullptr;
}

VerifyReport verifyInGame(pf::InputTape const& tape, int maxFrames) {
	VerifyReport report;

	auto* pl = PlayLayer::get();
	if (!pl || !pl->m_player1) {
		report.outcome = VerifyOutcome::Unavailable;
		return report;
	}

	// Start from a clean run of the level.
	g_killer.clear();
	g_verifying = true;

	pl->resetLevelFromStart();

	// Physics runs at 240 Hz regardless of the display refresh rate, and the
	// tape is expressed in those frames, so step at exactly that rate rather
	// than at whatever the game is currently rendering at.
	constexpr float kStep = 1.f / 240.f;

	size_t nextToggle = 0;
	bool pressed = false;
	int frame = 0;

	for (; frame < maxFrames; ++frame) {
		// Apply every toggle scheduled for this frame before stepping it.
		while (nextToggle < tape.toggles.size()
			   && tape.toggles[nextToggle] == static_cast<uint32_t>(frame)) {
			pressed = !pressed;
			++nextToggle;

			// Drive the real input path so the game applies its own buffering
			// and per-vehicle click semantics, rather than us poking state.
			pl->handleButton(pressed, 1, true);
		}

		pl->update(kStep);

		if (pl->m_player1->m_isDead || g_killer.valid)
			break;

		if (pl->m_hasCompletedLevel)
			break;
	}

	auto pos = pl->m_player1->getPosition();
	report.frame = frame;
	report.x = pos.x;
	report.y = pos.y;

	if (g_killer.valid || pl->m_player1->m_isDead) {
		report.outcome = VerifyOutcome::Died;
		report.killerObjectId = g_killer.objectId;
		report.killerType = g_killer.objectType;
		report.killerX = g_killer.x;
		report.killerY = g_killer.y;
	} else if (pl->m_hasCompletedLevel) {
		report.outcome = VerifyOutcome::Completed;
	} else {
		report.outcome = VerifyOutcome::TimedOut;
	}

	g_verifying = false;
	g_killer.clear();

	return report;
}
