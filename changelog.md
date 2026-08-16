# v2.0.0

Large rewrite of the simulator and the search. Renamed to **Path Finding**.

## New gameplay support
 * **Robot**, **spider** and **swing** gamemodes
 * **Duals** - the second player is simulated and the attempt fails if either dies
 * **Dash orbs**, **pink dash orbs** and **spider orbs**
 * **Teleport portals** (classic blue/orange pair)
 * **Red pads** - the physics were already there, the object id was never registered

## New search
 * Runs on every CPU core instead of one
 * Samples realistic click schedules per gamemode instead of uniform noise, so
   held inputs (dash orbs, robot boosts, long ship thrusts) actually get tried
 * Refines the best candidate by jittering its timings
 * Never commits to a state that dies shortly after, which is what caused macros
   to look finished and then die at the first jump
 * Backtracks to earlier checkpoints with growing distance when stuck
 * Deterministic - the same level searches the same way every time

## Correctness
 * Exported macros are replayed from frame 0 and verified before being written;
   the UI now says **Solved** or **Incomplete** instead of always looking finished
 * Fixed copied levels sharing the original's state history, which corrupted the
   exported input list
 * Fixed `Player` leaving `buffer`, `gravityPortal`, `dt` and `level` uninitialised
 * Fixed a crash on levels with no recognised objects
 * Fixed objects in the first and last section colliding twice per frame
 * Fixed the out of bounds check comparing a Y position against the level length
 * Frame numbers are 32 bit, so levels longer than 4m33s no longer wrap
 * Unsupported objects in a level are now named in the UI and the log

## Android
 * Export no longer uses the file picker, which scoped storage blocks
   (camila314/pathfinder#10, geode-sdk/geode#1287)
 * Macros are written to `game/macros/<level>.gdr2` with a notification
