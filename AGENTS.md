# AGENTS.md

Pathfinder ("Pathfinding Pro") — a Geode mod (C++20) that runs a 240 Hz discrete-step Geometry Dash physics simulation to auto-solve levels and export frame-exact `.gdr2` bot macros.

## Build

- No build tooling (cmake/compiler) is installed in this workspace and `$GEODE_SDK` is unset, so local builds cannot run here. Builds go through GitHub Actions **Build Geode Mod** (`multi-platform.yml`, Debug variant: `multi-platform-debug.yml`) — both are `workflow_dispatch`-only (push triggers are commented out). The artifact is the `camila314.pathfinder.geode` file in the `Build Output` ZIP.
- CMake requires `$GEODE_SDK` to point at a Geode SDK checkout or it aborts with `FATAL_ERROR`. Deps (`UIBuilder`, `GDReplayFormat`) are pulled by the SDK's bundled CPM — there is no local `cmake/CPM.cmake`.
- Platform matrix: Windows / macOS (`CMAKE_OSX_ARCHITECTURES arm64;x86_64`) / Android 32+64.
- Release flags matter: `-O3 -fno-math-errno -fno-trapping-math -fomit-frame-pointer`. Don't casually change optimization flags — the sim is calibrated to be frame-exact with real GD.
- Mod boilerplate lives in `src/main.cpp` (UI/PathfinderNode) and `src/pathfinder.cpp` (solver entry `pathfind()` → `PathfindResult` in `src/pathfinder.hpp`). `src/checker.cpp` is dead code (fully commented out) — don't treat it as the verification path.

## Testing / simulation

- `gd-sim/` is a dependency-free physics engine — read `gd-sim/include/Level.hpp` first to understand how the sim works; all calibrated constants live in `gd-sim/include/Physics.hpp` (240 Hz `PHYS_FPS`, `PHYS_DT = 1/240`, `PHYS_*` per-vehicle constants calibrated to GD 2.2 / Cocos2d-x 32-bit float precision).
- Standalone verification binary, only builds outside Android/Geode targets:
  ```
  cmake -S gd-sim -B build/gd-sim -DBUILD_GD_SIM_TEST=ON
  cmake --build build/gd-sim
  ./build/gd-sim/gd-sim-test <level-file> <input-file>
  ```
  (`gd-sim/test/test.cpp` — input file is a `'0'/'1'` string; prints "Macro failed at frame N".)
- In a `DEBUG_MODE` in-game build, `src/debug.cpp` verifies macros by shelling out to that same `build/gd-sim/gd-sim-test` binary (vendored `src/subprocess.hpp`); non-debug builds run the sim in-memory instead. `src/subprocess.hpp` is vendored cpp-subprocess (MIT) — don't edit it.
- Physics behavior changes must stay frame-exact; validate against the real game, not just the sim.

## Conventions / gotchas

- `mod.json`: id `camila314.pathfinder`, GD 2.2081, geode 5.10.0, requires `geode.node-ids >= v1.18.0`; version is a `v1.0.0-beta.N` counter mirrored in `changelog.md` beta notes.
- Replay export uses GDReplayFormat (`gdr2`); if `eclipse.eclipse-menu` is loaded, replays go into eclipse's `replays` dir.
- World Y is offset `+105.0` from editor floor bounds; velocity lookups index into 5-speed arrays (`PHYS_SPEEDS[0..4]` = 0.5x/1x/2x/3x/4x). Teleport-portal group parsing has been a repeated source of bugs — keep portal group mappings in sync with GD object data.
- Object physics is split per object type under `gd-sim/src/Objects/` (Block, Orb, Pad, Slope, Portals, etc.).