# v2.1.1

 * Exports now go to **Eclipse's replay folder** when Eclipse Menu is present:
   `save/geode/mods/eclipse.eclipse-menu/replays/<level>.gdr2`, which is the
   folder its replay browser actually reads. Detected by load state *and* by
   path, so a temporarily disabled Eclipse still gets its macros.
 * Without Eclipse the macro goes to `game/macros/` on Android as before.
 * Desktop and Android now pick the destination the same way; only the write
   differs (direct write vs file picker).

# v2.1.0

## Click Between Frames support
 * Detects **Click Between Frames** (`syzzi.click_between_frames`) and
   **CBF Extrapolate** (`square3ang.cbfextrapolate`). Those mods apply inputs at
   their true sub-frame time, so a macro that only works frame-exact can die
   with them on.
 * New hardening pass: after a solution is found, the plan is replayed with
   every input shifted a frame early, a frame late, and with random per-input
   jitter. If any of those fail, the timings around the failure are nudged until
   they stop failing. Hardening is forced on when CBF is installed.
 * The result screen now reports **timing slack** - the share of jittered
   replays that still finish. 100% means every input can land a frame early or
   late and the macro still works.

## Click rate
 * New **Max CPS** setting, default **15**. Presses are kept at least
   240/cps frames apart during the search, not filtered afterwards, so the cap
   holds across the whole macro including round boundaries.
 * New **Minimum click length** setting (default 1 frame) and **Harden timings**
   toggle.
 * Peak clicks per second is measured over a one second window and shown.

## Measured
 * 13 test levels, all solved and verified. Hardening lifts timing slack from
   38% to 100% on tight cube chains and 38% to 75% on a narrow ship corridor,
   for about 0.3s of extra work.

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
