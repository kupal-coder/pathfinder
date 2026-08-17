#pragma once
#include "search.hpp"
#include "pathfinder.hpp"
#include "cps.hpp"

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <functional>
#include <memory>
#include <thread>
#include <unordered_set>
#include <vector>

/**
 * The search engine.
 *
 * Header-only and free of Geode headers so that the identical code runs inside
 * the mod and inside the standalone benchmark binaries.
 *
 * Strategy: parallel best-first search over deduplicated physics states.
 *
 * An open list holds every state worth revisiting, ordered by score. Each round
 * the best few are expanded by simulating a chunk of frames under a set of
 * input patterns; surviving children are deduplicated and pushed back into the
 * open list.
 *
 * Using a persistent open list rather than a rolling beam is what allows the
 * search to recover from a dead end. A rolling beam that runs into a section it
 * cannot pass has nowhere to go -- re-expanding the same states is
 * deterministic and yields the same failures -- so it stalls. With an open list
 * the next-best state from an earlier point is simply picked up instead, which
 * makes backtracking a natural consequence of the ordering rather than a
 * special case.
 *
 * Three things make it fast enough to be practical:
 *
 *  - checkpoints instead of full history copies, so forking a candidate costs
 *    well under a microsecond instead of ~911us;
 *  - deduplication by bucketed physics state, so the open list holds distinct
 *    situations rather than many copies of one trajectory;
 *  - an adaptive frontier width that stays narrow while progress is easy and
 *    widens only when the search is stuck, so simple levels stay quick.
 */
namespace pf {

class Engine {
public:
	Engine(std::string const& levelString, PathfindOptions const& opts)
		: m_options(opts) {
		unsigned hw = std::thread::hardware_concurrency();
		if (hw == 0)
			hw = 2;
		m_threads = opts.threads ? opts.threads : std::max(1u, hw);

		// One Level per worker: the physics writes through Player::level, so
		// workers must not share one.
		m_levels.reserve(m_threads);
		for (unsigned i = 0; i < m_threads; ++i)
			m_levels.push_back(std::make_unique<Level>(levelString));

		m_length = m_levels.front()->length;
		m_objectCount = m_levels.front()->objectCount;
		m_unknown = m_levels.front()->unknownObjects;
	}

	float length() const { return m_length; }
	size_t objectCount() const { return m_objectCount; }

	/// Objects simulated with a guessed hitbox because their id is unknown.
	UnknownObjectLog const& unknownObjects() const { return m_unknown; }

	struct Outcome {
		InputTape tape;
		bool solved = false;
		double percent = 0.0;
		uint64_t framesSimulated = 0;
		uint64_t rounds = 0;
	};

	Outcome run(std::atomic_bool& stop,
				std::function<void(PathfindProgress const&)> const& callback) {
		Outcome outcome;
		if (m_length <= 0.0f)
			return outcome;

		std::vector<Candidate> open;
		{
			auto& lvl = *m_levels.front();
			Candidate start;
			start.checkpoint = lvl.checkpoint();
			start.pressed = false;
			start.frame = lvl.currentFrame();
			start.x = lvl.latestState().pos.x;
			start.score = scoreState(lvl.latestState());
			start.key = keyFor(lvl.latestState());
			open.push_back(std::move(start));
		}

		std::unordered_set<VisitKey, VisitKeyHash> visited;
		visited.reserve(1 << 16);

		double bestX = open.front().x;
		InputTape bestTape = open.front().tape;
		uint64_t stalled = 0;
		uint64_t rounds = 0;
		uint64_t framesSimulated = 0;
		unsigned width = m_minWidth;

		PathfindProgress progress;

		while (!stop && !open.empty()) {
			// Take the best `width` states as this round's frontier.
			unsigned take = std::min<unsigned>(width, static_cast<unsigned>(open.size()));
			std::partial_sort(open.begin(), open.begin() + take, open.end(),
							  [](Candidate const& a, Candidate const& b) { return a.score > b.score; });

			std::vector<Candidate> frontier(std::make_move_iterator(open.begin()),
											std::make_move_iterator(open.begin() + take));
			open.erase(open.begin(), open.begin() + take);

			// Switch to exhaustive expansion only once simple moves stop paying off.
			bool thorough = stalled >= 2;
			auto children = expandFrontier(frontier, framesSimulated, stop, thorough);
			if (stop)
				break;

			++rounds;

			bool improved = false;
			for (auto& c : children) {
				if (c.x >= m_length) {
					outcome.tape = c.tape;
					outcome.solved = true;
					outcome.percent = 100.0;
					outcome.framesSimulated = framesSimulated;
					outcome.rounds = rounds;

					progress.percent = 100.0;
					progress.solved = true;
					progress.rounds = rounds;
					progress.framesSimulated = framesSimulated;
					if (callback)
						callback(progress);
					return outcome;
				}

				if (!visited.emplace(VisitKey{c.key, c.frame / kVisitFrameBucket}).second)
					continue;

				if (c.x > bestX) {
					bestX = c.x;
					bestTape = c.tape;
					improved = true;
				}
				open.push_back(std::move(c));
			}

			// Adaptive width: stay narrow while progress is easy so simple
			// levels finish quickly, widen when stuck so hard sections get the
			// exploration they need.
			if (improved) {
				stalled = 0;
				width = m_minWidth;
			} else {
				++stalled;
				if (stalled % 4 == 0)
					width = std::min<unsigned>(width * 2, m_options.beamWidth);
			}

			pruneOpen(open);

			progress.percent = std::min(100.0, bestX / m_length * 100.0);
			progress.frontierPercent = open.empty()
				? progress.percent
				: std::min(100.0, open.front().x / m_length * 100.0);
			progress.rounds = rounds;
			progress.framesSimulated = framesSimulated;
			progress.stalledRounds = stalled;
			progress.frontier = static_cast<uint32_t>(open.size());
			if (callback)
				callback(progress);

			if (m_options.stallLimit && stalled >= m_options.stallLimit)
				break;
		}

		outcome.tape = bestTape;
		outcome.solved = false;
		outcome.percent = std::min(100.0, bestX / m_length * 100.0);
		outcome.framesSimulated = framesSimulated;
		outcome.rounds = rounds;
		return outcome;
	}

private:
	/**
	 * Frame resolution of the visited set.
	 *
	 * X position is a direct function of frame, so this doubles as a position
	 * bucket: at 1x speed a frame is ~1.6 units. Coarse values quietly merge
	 * states that are far apart horizontally -- at 16 frames that is ~26 units,
	 * more than half a block, which discards the precise approach a tight jump
	 * needs and empties the open list. 1 keeps states distinct.
	 */
	static constexpr int kVisitFrameBucket = 1;

	struct VisitKey {
		StateKey state;
		int frameBucket;
		bool operator==(VisitKey const& o) const {
			return frameBucket == o.frameBucket && state == o.state;
		}
	};
	struct VisitKeyHash {
		size_t operator()(VisitKey const& k) const {
			size_t h = StateKeyHash{}(k.state);
			h ^= static_cast<size_t>(k.frameBucket) * 1099511628211ull;
			return h;
		}
	};

	/**
	 * Input patterns tried per candidate.
	 *
	 * Hold and release cover the constant cases. A single toggle at each frame
	 * covers "start or stop pressing here". Short pulses -- press then release
	 * a few frames later -- matter because a cube jump is a press/release pair
	 * that often has to complete inside one chunk; without them a tight jump
	 * can only be expressed by splitting across two rounds, which the frame
	 * bucketing may merge away.
	 */
	std::vector<Candidate> expandFrontier(std::vector<Candidate> const& frontier,
										  uint64_t& framesSimulated,
										  std::atomic_bool const& stop,
										  bool thorough) {
		int chunk = static_cast<int>(m_options.chunkFrames);

		// Pattern encoding: first toggle offset, then optional second offset.
		struct Pattern {
			int first;   // -2 hold, -1 release, >=0 toggle at offset
			int second;  // -1 none, else second toggle offset
		};
		/*
		 * Pattern density adapts to how well the search is doing.
		 *
		 * While progress is steady a sparse set is enough, and it is far
		 * cheaper: an easy level does not need every possible tap enumerated,
		 * and exploring them anyway made simple levels dramatically slower than
		 * a naive greedy search. When the search stalls the full set is used,
		 * which is what gets it through a section that needs exact timing.
		 */
		int step = thorough ? 1 : 4;

		/*
		 * Click rate cap.
		 *
		 * A press/release pair closer together than this would exceed 70 CPS,
		 * which no real bot can reproduce. The interval comes from a fixed
		 * 1/70 s timestep rather than the frame count, so the limit means the
		 * same thing at any physics rate. See cps.hpp.
		 */
		int const minGap = pf::minToggleInterval();

		std::vector<Pattern> patterns;
		patterns.reserve(static_cast<size_t>(chunk) * 4 + 2);
		patterns.push_back({-2, -1});
		patterns.push_back({-1, -1});
		for (int f = 0; f < chunk; f += step) {
			patterns.push_back({f, -1});
			if (thorough) {
				for (int d : {2, 4, 8}) {
					if (d >= minGap && f + d < chunk)
						patterns.push_back({f, f + d});
				}
			} else {
				// A cube jump is a press/release pair; keep one short pulse
				// even in the sparse set so jumps remain reachable in a single
				// round.
				int pulse = std::max(4, minGap);
				if (f + pulse < chunk)
					patterns.push_back({f, f + pulse});
			}
		}

		std::vector<std::vector<Candidate>> perWorker(m_threads);
		std::vector<uint64_t> framesPerWorker(m_threads, 0);
		std::atomic<size_t> cursor{0};

		auto work = [&](unsigned workerIdx) {
			auto& lvl = *m_levels[workerIdx];
			auto& out = perWorker[workerIdx];
			uint64_t frames = 0;

			while (!stop) {
				size_t i = cursor.fetch_add(1);
				if (i >= frontier.size())
					break;
				auto const& parent = frontier[i];

				for (auto const& pat : patterns) {
					if (stop)
						break;

					lvl.restore(parent.checkpoint);
					bool pressed = parent.pressed;
					InputTape tape = parent.tape;

					// Reject a pattern whose first toggle would land too soon
					// after the parent's last one, across the chunk boundary.
					if (pat.first >= 0 &&
						tape.wouldExceedRate(
							static_cast<uint32_t>(parent.frame + pat.first), minGap))
						continue;

					if (pat.first == -2 && !pressed) {
						if (tape.wouldExceedRate(static_cast<uint32_t>(parent.frame), minGap))
							continue;
						pressed = true;
						tape.toggle(static_cast<uint32_t>(parent.frame));
					} else if (pat.first == -1 && pressed) {
						if (tape.wouldExceedRate(static_cast<uint32_t>(parent.frame), minGap))
							continue;
						pressed = false;
						tape.toggle(static_cast<uint32_t>(parent.frame));
					}

					bool died = false;
					for (int f = 0; f < chunk; ++f) {
						if (f == pat.first || f == pat.second) {
							pressed = !pressed;
							tape.toggle(static_cast<uint32_t>(lvl.currentFrame()));
						}
						lvl.runFrame(pressed);
						++frames;
						if (lvl.latestState().dead) {
							died = true;
							break;
						}
					}

					auto const& st = lvl.latestState();
					if (died || isDoomed(st, m_length))
						continue;

					Candidate child;
					child.checkpoint = lvl.checkpoint();
					child.tape = std::move(tape);
					child.pressed = pressed;
					child.x = st.pos.x;
					child.frame = lvl.currentFrame();
					child.score = scoreState(st);
					child.key = keyFor(st);
					out.push_back(std::move(child));
				}
			}
			framesPerWorker[workerIdx] = frames;
		};

		if (m_threads == 1) {
			work(0);
		} else {
			std::vector<std::thread> pool;
			pool.reserve(m_threads);
			for (unsigned t = 0; t < m_threads; ++t)
				pool.emplace_back(work, t);
			for (auto& t : pool)
				t.join();
		}

		std::vector<Candidate> merged;
		size_t total = 0;
		for (auto const& v : perWorker)
			total += v.size();
		merged.reserve(total);
		for (auto& v : perWorker)
			for (auto& c : v)
				merged.push_back(std::move(c));
		for (auto f : framesPerWorker)
			framesSimulated += f;
		return merged;
	}

	/**
	 * Bound the open list.
	 *
	 * Each candidate carries a checkpoint (a window of Player states), so an
	 * unbounded list would grow into gigabytes on a long search. Keeping the
	 * best `kMaxOpen` preserves the useful frontier -- including older, lower
	 * scoring states that backtracking depends on -- while capping memory at a
	 * few tens of megabytes.
	 */
	void pruneOpen(std::vector<Candidate>& open) const {
		if (open.size() <= kMaxOpen)
			return;
		std::partial_sort(open.begin(), open.begin() + kMaxOpen, open.end(),
						  [](Candidate const& a, Candidate const& b) { return a.score > b.score; });
		open.resize(kMaxOpen);
	}

	static constexpr size_t kMaxOpen = 4096;

	PathfindOptions m_options;
	unsigned m_threads = 1;
	unsigned m_minWidth = 4;
	std::vector<std::unique_ptr<Level>> m_levels;
	float m_length = 0.0f;
	size_t m_objectCount = 0;
	UnknownObjectLog m_unknown;
};

} // namespace pf
