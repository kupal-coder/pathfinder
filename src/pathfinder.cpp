#include "pathfinder.hpp"
#include "engine.hpp"
#include "search.hpp"

#include <Level.hpp>
#include <gdr/gdr.hpp>

#include <exception>

class Replay2 : public gdr::Replay<Replay2, gdr::Input<"">> {
 public:
	Replay2() : Replay("Path Finding Pro", 1) {}
};

/**
 * Convert a solved input tape into a .gdr2 replay.
 *
 * Frames are 32-bit throughout. They used to be held in a `std::set<uint16_t>`,
 * which silently wrapped past frame 65535 -- 4.55 minutes at 240fps -- so on a
 * long level the planned inputs landed at nonsense frames near the start and
 * quietly did nothing.
 */
static std::vector<uint8_t> buildReplay(pf::InputTape const& tape) {
	Replay2 output;
	output.inputs.reserve(tape.toggles.size());

	bool pressed = false;
	for (uint32_t frame : tape.toggles) {
		pressed = !pressed;
		output.inputs.push_back(gdr::Input(frame, 1, false, pressed));
	}

	return output.exportData().unwrapOr({});
}

PathfindResult pathfind(
	std::string const& lvlString,
	std::atomic_bool& stop,
	std::function<void(PathfindProgress const&)> callback,
	PathfindOptions const& options) {

	PathfindResult result;

	try {
		pf::Engine engine(lvlString, options);

		if (engine.objectCount() == 0) {
			result.error = "This level has no objects the simulator understands.";
			return result;
		}

		auto outcome = engine.run(stop, callback);

		result.solved = outcome.solved;
		result.percent = outcome.percent;
		result.macro = buildReplay(outcome.tape);

		if (result.macro.empty() && !outcome.tape.toggles.empty())
			result.error = "Failed to encode the macro.";
		else if (!outcome.solved && !stop)
			result.error = "No route found. The simulator ran out of options at "
						 + std::to_string(static_cast<int>(outcome.percent)) + "%.";

		return result;
	} catch (std::exception const& ex) {
		result.error = std::string("Pathfinding failed: ") + ex.what();
		return result;
	} catch (...) {
		result.error = "Pathfinding failed with an unknown error.";
		return result;
	}
}
