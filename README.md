# Path Finding Pro
# Geometry Dash Physics Simulator

Full support up to 1.7, partial up to 1.9. This code is not currently licensed for redistribution.

## Notes

1. Y positions are slightly off because in the real gd, they are offest by 105

## Repository layout

| Path | What it is |
|---|---|
| `gd-sim/` | The physics simulator. Builds standalone, no Geode SDK required. |
| `src/search.hpp` | Search primitives: input tapes, state keys, scoring. |
| `src/engine.hpp` | The search engine. Header-only, no Geode dependency. |
| `src/pathfinder.cpp` | Bridges the engine to the `.gdr2` replay format. |
| `src/replay.hpp` | Shared `.gdr2` replay type used by every component. |
| `src/library.cpp` | The macro library: save, list, rename, delete, export. |
| `src/LibraryPopup.cpp` | The in-game macro library browser. |
| `src/main.cpp` | The in-game UI. |
| `src/debug.cpp` | Editor overlay for comparing simulated and real trajectories. |

## Building and testing without the game

The simulator and the search engine have no dependency on Geode or on GD, so
they can be built and tested on any machine:

```bash
cmake -S gd-sim -B build-sim -DCMAKE_BUILD_TYPE=Release
cmake --build build-sim -j"$(nproc)"

./build-sim/gd-sim-golden verify   # physics regression traces
./build-sim/gd-sim-searchtest      # search correctness
./build-sim/gd-sim-librarytest     # macro library naming rules
./build-sim/gd-sim-searchbench     # search benchmark gauntlet
```

`ctest` runs the first three. CI runs all four, plus an ASan/UBSan build.

Note that `librarytest` keeps its own copy of the naming logic from
`src/library.cpp` so it can run without Geode. If you change the sanitising or
de-duplication rules, change both.

### Golden traces

`gd-sim-golden` replays a set of scenarios covering blocks, spikes, slopes,
orbs, pads, every portal type and all five vehicles, and compares the result
against recorded traces in `gd-sim/test/golden/`.

Each trace carries a rolling checksum folded from every field of every frame,
so any drift is caught even where the readable lines are sampled. After an
intentional physics change, re-record:

```bash
./build-sim/gd-sim-golden record
```

and review the diff before committing.

### Benchmarking a real level

`gd-sim-searchbench` accepts a file containing a decompressed level string:

```bash
./build-sim/gd-sim-searchbench mylevel.txt
```

Set `PF_THREADS` to pin the worker count.

## How the search works

A parallel best-first search over deduplicated physics states.

An open list holds every state worth revisiting, ordered by score. Each round
the best few are expanded by simulating a chunk of frames under a set of input
patterns; surviving children are bucketed by physics state so duplicates
collapse, then pushed back onto the open list.

Three things make it practical:

- **Bounded history.** The simulator keeps a small window of player states
  rather than every frame, so branching costs well under a microsecond. See
  `gd-sim/include/StateHistory.hpp`.
- **State deduplication.** Candidates are keyed on bucketed position, velocity,
  vehicle and flags, so the frontier holds distinct situations instead of many
  copies of one trajectory.
- **Adaptive effort.** Expansion stays sparse while progress is steady and
  becomes exhaustive only when the search stalls.

Because the open list is persistent, backtracking falls out of the ordering:
when a route dies the next-best earlier state is simply picked up instead.
