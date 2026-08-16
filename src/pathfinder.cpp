#include <Level.hpp>
#include <gdr/gdr.hpp>
#include "pathfinder.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <random>
#include <thread>

/*
	The search
	----------
	The old search kept one chain of play, threw 30 uniformly random toggles into
	the next 1000 frames, kept whichever of 300 tries survived longest, and
	committed two thirds of it. That has three problems:

	  * uniform toggles are nothing like real play, so flight modes (which need
	    long holds) and orb timings almost never come up by chance;
	  * it commits to a run purely because it survived longest, even when the
	    state it commits to is already doomed a few frames later;
	  * it is single threaded, so it can only afford a few hundred tries.

	This version:

	  * samples *click schedules* (alternating hold/gap durations) whose
	    distribution depends on the current gamemode, plus explicit "do nothing"
	    and "hold everything" candidates;
	  * evaluates candidates on every core;
	  * refines the best candidate by jittering its toggles a few frames;
	  * never commits within `safety_margin` frames of a death, so the committed
	    state always has a proven future;
	  * backtracks to earlier checkpoints with growing distance when stuck;
	  * replays the exported inputs from frame 0 and checks they still finish
	    before reporting success.
*/

namespace {

class Replay2 : public gdr::Replay<Replay2, gdr::Input<"">> {
 public:
	Replay2() : Replay("Path Finding", 2) {}
};

constexpr int horizon_frames = 900;
constexpr int safety_margin = 96;
constexpr int base_candidates = 192;
constexpr int refinements = 64;
constexpr int min_commit = 24;
constexpr int stall_rounds = 5;
constexpr int max_threads = 4;

using Schedule = std::vector<uint32_t>;

struct Outcome {
	uint32_t endFrame = 0;
	float maxX = 0;
	bool reachedEnd = false;
	bool died = false;
};

bool flying(VehicleType v) {
	return v == VehicleType::Ship || v == VehicleType::Ufo
		|| v == VehicleType::Wave || v == VehicleType::Swing;
}

/// Builds one plausible click schedule for the next `horizon` frames.
Schedule sampleSchedule(std::mt19937& rng, uint32_t start, int horizon, VehicleType vehicle, int aggression) {
	Schedule out;
	std::uniform_int_distribution<int> pick(0, 99);

	// Gaps and holds are drawn from ranges that suit the current gamemode. Real
	// play is bursts of holds separated by gaps, never uniform noise.
	int gapMin, gapMax, holdMin, holdMax;
	if (flying(vehicle)) {
		gapMin = 2; gapMax = 46; holdMin = 2; holdMax = 64;
	} else if (vehicle == VehicleType::Robot) {
		gapMin = 6; gapMax = 90; holdMin = 1; holdMax = 40;
	} else if (vehicle == VehicleType::Ball || vehicle == VehicleType::Spider) {
		gapMin = 6; gapMax = 80; holdMin = 1; holdMax = 10;
	} else {
		gapMin = 4; gapMax = 96; holdMin = 1; holdMax = 12;
	}

	// More aggression means denser, twitchier input for the parts that need it.
	gapMax = std::max(gapMin + 1, gapMax - aggression * 8);

	std::uniform_int_distribution<int> gapDist(gapMin, gapMax);
	std::uniform_int_distribution<int> holdDist(holdMin, holdMax);

	uint32_t f = start + (uint32_t)gapDist(rng);
	std::uniform_int_distribution<int> longHold(40, 400);

	while (f < start + horizon) {
		int hold = holdDist(rng);

		// occasional very long hold, which is what most ship/wave sections want
		if (flying(vehicle) && pick(rng) < 18)
			hold += holdDist(rng) + holdDist(rng);

		// Every mode needs the occasional sustained hold, not just the flying
		// ones: dash orbs, green orbs and robot boosts are all held inputs, and
		// a tap-only distribution can never express them.
		if (pick(rng) < 14)
			hold += longHold(rng);

		out.push_back(f);
		f += hold;
		if (f >= start + horizon)
			break;

		out.push_back(f);
		f += gapDist(rng);
	}

	return out;
}

/// Runs `lvl` forward from its current frame, applying `sched`.
Outcome evaluate(Level& lvl, bool press, Schedule const& sched, uint32_t start, int horizon, float endX) {
	Outcome o;
	o.maxX = lvl.latestState().pos.x;

	size_t next = 0;
	while ((uint32_t)lvl.currentFrame() < start + (uint32_t)horizon) {
		uint32_t frame = (uint32_t)lvl.currentFrame();
		while (next < sched.size() && sched[next] <= frame) {
			press = !press;
			++next;
		}

		lvl.runFrame(press);

		auto const& st = lvl.latestState();
		o.maxX = std::max(o.maxX, st.pos.x);

		if (lvl.anyDead()) {
			o.died = true;
			break;
		}
		if (st.pos.x >= endX) {
			o.reachedEnd = true;
			break;
		}
	}

	o.endFrame = (uint32_t)lvl.currentFrame();
	return o;
}

/// Advances `lvl` to `until`, applying any toggles in `sched` on the way.
bool advance(Level& lvl, bool& press, Schedule const& sched, uint32_t until, float endX) {
	size_t next = 0;
	while ((uint32_t)lvl.currentFrame() < until) {
		uint32_t frame = (uint32_t)lvl.currentFrame();
		while (next < sched.size() && sched[next] <= frame) {
			press = !press;
			++next;
		}
		lvl.runFrame(press);
		if (lvl.anyDead())
			return false;
		if (lvl.latestState().pos.x >= endX)
			return true;
	}
	return true;
}

/// Replays a whole plan onto a fresh level and reports the button state it ends
/// on. Used for backtracking and for the final verification pass.
Level materialise(std::string const& lvlString, Schedule const& plan, uint32_t until, float endX, bool& press) {
	Level lvl(lvlString);
	press = false;
	advance(lvl, press, plan, until, endX);
	return lvl;
}

/// Names for the object ids that change how a level plays. Anything else that is
/// unrecognised is almost always decoration, and is only counted.
std::string describeObject(int id) {
	static std::map<int, char const*> const names = {
		{ 45,   "mirror portal" },      { 46,   "mirror portal" },
		{ 899,  "colour trigger" },     { 901,  "move trigger" },
		{ 1006, "pulse trigger" },      { 1007, "alpha trigger" },
		{ 1049, "toggle trigger" },     { 1268, "spawn trigger" },
		{ 1346, "rotate trigger" },     { 1347, "follow trigger" },
		{ 1520, "shake trigger" },      { 1585, "animate trigger" },
		{ 1611, "count trigger" },      { 1616, "stop trigger" },
		{ 1811, "instant count trigger" },
		{ 1814, "follow player Y trigger" },
		{ 1817, "collision trigger" },  { 1912, "random trigger" },
		{ 1913, "camera zoom trigger" },{ 1917, "reverse trigger" },
		{ 1932, "end trigger" },        { 1935, "time warp trigger" },
		{ 2015, "rotate gameplay trigger" },
		{ 2062, "scale trigger" },      { 2066, "gravity trigger" },
		{ 2067, "advanced follow trigger" },
		{ 2899, "options trigger" },    { 2900, "gradient trigger" },
		{ 2901, "teleport trigger" },   { 2902, "teleport trigger" },
		{ 2903, "camera trigger" },     { 2904, "camera trigger" },
		{ 3606, "toggle orb" },         { 3607, "teleport orb" },
		{ 3612, "gravity trigger" },
	};

	if (auto it = names.find(id); it != names.end())
		return it->second;
	return {};
}

std::vector<std::string> describeUnknown(Level const& lvl) {
	std::map<std::string, int> named;
	int decoration = 0;

	for (auto const& [id, count] : lvl.unknownObjects) {
		auto name = describeObject(id);
		if (name.empty())
			decoration += count;
		else
			named[name] += count;
	}

	std::vector<std::string> out;
	for (auto const& [name, count] : named)
		out.push_back(name + " x" + std::to_string(count));

	if (!out.empty() && decoration > 0)
		out.push_back(std::to_string(decoration) + " other unsimulated objects");

	return out;
}

}  // namespace

PathfindResult pathfind(
	std::string const& lvlString,
	std::atomic_bool& stop,
	std::function<void(double)> callback,
	std::string const& levelName
) {
	PathfindResult result;

	Level base(lvlString);
	result.warnings = describeUnknown(base);

	float const endX = base.length;
	if (endX <= 0) {
		result.warnings.push_back("no simulatable objects were found");
		return result;
	}

	unsigned threadCount = std::thread::hardware_concurrency();
	if (threadCount == 0)
		threadCount = 1;
	threadCount = std::min<unsigned>(threadCount, max_threads);

	// Deterministic: the same level always searches the same way.
	std::seed_seq seed{ (uint32_t)std::hash<std::string>{}(lvlString), 0x9e3779b9u };
	std::mt19937 rng(seed);

	Schedule plan;                 // committed toggles
	Level current = base;
	bool press = false;

	Schedule bestPlan;
	float bestX = 0;
	uint32_t bestFrame = 1;

	int stalls = 0;
	int aggression = 0;
	uint32_t backoff = 240;

	while (!stop) {
		if (current.latestState().pos.x >= endX)
			break;

		uint32_t const start = (uint32_t)current.currentFrame();
		auto const vehicle = current.latestState().vehicle.type;

		// ---- build candidates (single threaded, so the run stays reproducible)
		int candidateCount = base_candidates + aggression * 64;
		std::vector<Schedule> candidates;
		candidates.reserve(candidateCount + 2);
		candidates.push_back({});                                   // do nothing
		candidates.push_back({ start });                            // hold everything

		// "click here, then keep holding" - the shape every held mechanic needs
		// (dash orbs, robot boosts, long ship thrusts).
		std::uniform_int_distribution<int> holdPoint(0, horizon_frames - 1);
		for (int i = 0; i < 12; ++i)
			candidates.push_back({ start + (uint32_t)holdPoint(rng) });

		for (int i = 0; i < candidateCount; ++i)
			candidates.push_back(sampleSchedule(rng, start, horizon_frames, vehicle, aggression));

		// ---- evaluate in parallel
		std::vector<Outcome> outcomes(candidates.size());
		auto runRange = [&](unsigned tid) {
			Level lvl = current;
			for (size_t i = tid; i < candidates.size(); i += threadCount) {
				lvl.rollback((int)start);
				outcomes[i] = evaluate(lvl, press, candidates[i], start, horizon_frames, endX);
			}
		};

		if (threadCount == 1) {
			runRange(0);
		} else {
			std::vector<std::thread> pool;
			for (unsigned t = 0; t < threadCount; ++t)
				pool.emplace_back(runRange, t);
			for (auto& t : pool)
				t.join();
		}

		// ---- pick the best, then try to improve it by jittering its toggles
		auto score = [&](Outcome const& o) {
			return o.reachedEnd ? 1e9f : o.maxX * 1000.0f + (float)o.endFrame;
		};

		size_t bestIdx = 0;
		for (size_t i = 1; i < outcomes.size(); ++i)
			if (score(outcomes[i]) > score(outcomes[bestIdx]))
				bestIdx = i;

		if (!outcomes[bestIdx].reachedEnd && !candidates[bestIdx].empty()) {
			std::vector<Schedule> tweaks;
			tweaks.reserve(refinements);
			std::uniform_int_distribution<int> jitter(-4, 4);
			for (int i = 0; i < refinements; ++i) {
				Schedule s = candidates[bestIdx];
				for (auto& f : s) {
					int nf = (int)f + jitter(rng);
					f = (uint32_t)std::max<int>(nf, (int)start);
				}
				std::sort(s.begin(), s.end());
				tweaks.push_back(std::move(s));
			}

			std::vector<Outcome> tweakOutcomes(tweaks.size());
			auto runTweaks = [&](unsigned tid) {
				Level lvl = current;
				for (size_t i = tid; i < tweaks.size(); i += threadCount) {
					lvl.rollback((int)start);
					tweakOutcomes[i] = evaluate(lvl, press, tweaks[i], start, horizon_frames, endX);
				}
			};
			if (threadCount == 1) {
				runTweaks(0);
			} else {
				std::vector<std::thread> pool;
				for (unsigned t = 0; t < threadCount; ++t)
					pool.emplace_back(runTweaks, t);
				for (auto& t : pool)
					t.join();
			}

			for (size_t i = 0; i < tweaks.size(); ++i) {
				if (score(tweakOutcomes[i]) > score(outcomes[bestIdx])) {
					candidates.push_back(tweaks[i]);
					outcomes.push_back(tweakOutcomes[i]);
					bestIdx = candidates.size() - 1;
				}
			}
		}

		Outcome const& best = outcomes[bestIdx];
		Schedule const& sched = candidates[bestIdx];

		// ---- decide how much of it is safe to keep
		uint32_t commitTo;
		if (best.reachedEnd) {
			commitTo = best.endFrame;
		} else {
			// Never commit into a state that dies almost immediately: back off
			// from the death by a fixed margin, and never keep more than two
			// thirds of the horizon so the next round can still react.
			uint32_t safeUntil = best.endFrame > (uint32_t)safety_margin
				? best.endFrame - safety_margin : start;
			commitTo = std::min<uint32_t>(start + horizon_frames * 2 / 3, safeUntil);
		}

		if (commitTo > start + min_commit) {
			for (auto f : sched)
				if (f < commitTo)
					plan.push_back(f);

			bool alive = advance(current, press, sched, commitTo, endX);

			float x = current.latestState().pos.x;
			if (x > bestX) {
				bestX = x;
				bestFrame = (uint32_t)current.currentFrame();
				bestPlan = plan;
				stalls = 0;
				aggression = std::max(0, aggression - 1);
				backoff = 240;
			} else {
				++stalls;
			}

			if (!alive) {
				// Should not happen thanks to the safety margin, but never keep
				// a dead state as the working state.
				++stalls;
				plan = bestPlan;
				current = materialise(lvlString, plan, bestFrame, endX, press);
			}

			if (current.latestState().pos.x >= endX)
				break;
		} else {
			++stalls;
		}

		// ---- stuck: rewind and try that section differently
		if (stalls >= stall_rounds) {
			stalls = 0;
			aggression = std::min(4, aggression + 1);

			uint32_t target = bestFrame > backoff ? bestFrame - backoff : 1;
			backoff = std::min<uint32_t>(backoff * 2, 20000);

			plan = bestPlan;
			plan.erase(std::remove_if(plan.begin(), plan.end(),
				[&](uint32_t f) { return f >= target; }), plan.end());

			current = materialise(lvlString, plan, target, endX, press);

			if (backoff >= 20000 && target <= 1)
				aggression = 0;
		}

		if (callback)
			callback(std::min(100.0f, (bestX / endX) * 100.0f));
	}

	// ------------------------------------------------------------------ export
	// An empty plan is a legitimate result: some sections (and some whole
	// levels) are cleared by holding nothing at all.
	bool const solved = current.latestState().pos.x >= endX;
	Schedule const winner = solved ? plan : bestPlan;

	// Replay the plan from frame 0 on a clean level. Everything exported comes
	// from this replay, so what is written out is exactly what was verified.
	Level replay(lvlString);
	bool rp = false;
	size_t next = 0;
	uint32_t const limit = std::min<uint32_t>(
		std::max<uint32_t>((winner.empty() ? 0u : winner.back()) + horizon_frames * 4u, 2000u),
		3'000'000u);

	while (!replay.anyDead()
		&& replay.latestState().pos.x < endX
		&& (uint32_t)replay.currentFrame() < limit) {

		uint32_t frame = (uint32_t)replay.currentFrame();
		while (next < winner.size() && winner[next] <= frame) {
			rp = !rp;
			++next;
		}
		replay.runFrame(rp);
	}

	Replay2 macro;
	macro.framerate = 240.0;
	macro.gameVersion = 22081;
	macro.author = "Path Finding";
	macro.description = "Generated by Path Finding";
	macro.levelInfo = gdr::Level(levelName);

	for (size_t i = 1; i < replay.gameStates.size(); ++i) {
		// Indexed directly instead of through prevPlayer(): a Player's `level`
		// pointer used to be able to reference a different Level's history.
		auto const& cur = replay.gameStates[i];
		auto const& prev = replay.gameStates[i - 1];
		if (cur.frame > 1 && cur.button != prev.button)
			macro.inputs.push_back(gdr::Input(cur.frame, 1, false, cur.button));
	}

	macro.sortInputs();
	macro.duration = (float)replay.currentFrame() / 240.0f;

	result.frames = (uint32_t)replay.currentFrame();
	result.clicks = (uint32_t)macro.inputs.size();
	result.progress = std::min(100.0f, (replay.latestState().pos.x / endX) * 100.0f);
	result.completed = replay.latestState().pos.x >= endX && !replay.anyDead();
	result.verified = result.completed;
	result.macro = macro.exportData().unwrapOr({});

	if (!result.completed && result.warnings.empty())
		result.warnings.push_back("the simulator could not get past " +
			std::to_string((int)result.progress) + "%");

	return result;
}
