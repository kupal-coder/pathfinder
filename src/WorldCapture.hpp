#pragma once
#include <vector>

#include "ObjectModel.hpp"

class GJBaseGameLayer;

/// Summary of what was captured, for reporting how well the world is modelled.
struct WorldCaptureStats {
	/// Total objects seen.
	size_t total = 0;
	/// Objects whose behaviour the search cannot model exactly.
	std::vector<int> unsupported;
	/// Objects whose GameObjectType was not recognised at all.
	size_t unclassified = 0;

	bool fullyModelled() const { return unsupported.empty() && unclassified == 0; }
};

/**
 * Snapshot the game's live object list into the search's own representation.
 *
 * Must be called on the main thread with a level loaded. Returns an empty
 * vector when no object list is available.
 *
 * Implemented in WorldCapture.cpp, the only translation unit besides
 * Verifier.cpp that touches game bindings, so the rest of the search stays
 * buildable and testable without Geometry Dash.
 */
std::vector<RuntimeObject> captureWorld(GJBaseGameLayer* layer, WorldCaptureStats* stats = nullptr);
