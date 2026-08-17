#pragma once
#include <cstdint>
#include <string>
#include <vector>

/**
 * Record of objects the simulator did not understand.
 *
 * This exists because of the phasing bug. `Object::create` returns an empty
 * optional for any object id it has no entry for, and the level parser silently
 * skipped those -- so a solid 2.2 block simply did not exist in the search
 * world. The search would happily plan a route straight through it, report
 * 100%, and the macro would die on the first frame that touched it.
 *
 * The fix has two halves:
 *
 *  1. Objects that are probably solid are no longer dropped. See
 *     `Object::create` and the fallback hitbox path.
 *  2. Anything still unrecognised is recorded here rather than discarded, so
 *     the search can refuse to claim a level is solved when it knowingly
 *     simulated an incomplete world, and the UI can say which ids are missing.
 *
 * Silently ignoring an object is never acceptable: it converts "we cannot
 * simulate this" into "this level is solved", which is the single worst
 * failure mode this mod has.
 */
struct UnknownObject {
	int objectId = 0;
	float x = 0.f;
	float y = 0.f;
};

struct UnknownObjectLog {
	std::vector<UnknownObject> entries;

	/// Ids seen, for a compact summary.
	std::vector<int> distinctIds() const {
		std::vector<int> out;
		for (auto const& e : entries) {
			bool seen = false;
			for (int id : out)
				if (id == e.objectId) { seen = true; break; }
			if (!seen)
				out.push_back(e.objectId);
		}
		return out;
	}

	bool empty() const { return entries.empty(); }
	size_t size() const { return entries.size(); }

	void add(int id, float x, float y) { entries.push_back({id, x, y}); }

	/// Short human-readable summary for logs and the UI.
	std::string summary(size_t maxIds = 6) const {
		if (entries.empty())
			return "";

		auto ids = distinctIds();
		std::string out = std::to_string(entries.size()) + " unsupported object";
		if (entries.size() != 1)
			out += "s";
		out += " (id";
		if (ids.size() != 1)
			out += "s";
		out += " ";

		for (size_t i = 0; i < ids.size() && i < maxIds; ++i) {
			if (i)
				out += ", ";
			out += std::to_string(ids[i]);
		}
		if (ids.size() > maxIds)
			out += ", +" + std::to_string(ids.size() - maxIds) + " more";
		out += ")";
		return out;
	}
};
