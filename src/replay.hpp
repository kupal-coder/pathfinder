#pragma once
#include <gdr/gdr.hpp>

/**
 * The replay type Path Finding Pro reads and writes.
 *
 * Defined once and shared by the solver, the macro library and the debug
 * overlay so that the bot name and input layout cannot drift between them.
 */
class PathfinderReplay : public gdr::Replay<PathfinderReplay, gdr::Input<"">> {
public:
	PathfinderReplay() : Replay("Path Finding Pro", 1) {}
};
