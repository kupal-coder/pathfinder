# Manual acceptance levels

These files are raw, uncompressed Geometry Dash level strings. They are bundled
with the mod and can be run from the **Tests** button beside Pathfinder in the
editor. The runner searches every fixture, performs two fresh runtime replays,
and writes `acceptance-report.txt` in the mod save directory. A task passes only
if both replays complete without safe mode and produce identical trace hashes.

## `object-*.lvl` (Task 1)

There is at least one focused runtime fixture for each requested feature: dual
players, all floor/ceiling slope directions, partial rotations in cube/ball/UFO,
robot holds, spider, swing, toggle/spawn/move trigger graphs, multiple dash-orb
directions, linked teleport portals, a 2.2 force block, and individual D/J/S/H
modifier checks. These deliberately rely on runtime `GameObject` behavior; no
production object-ID hitbox or behavior table is involved.

## `runtime-phasing.lvl` (Task 2)

The trigger graph moves group 7 into the player's route. The old standalone
world either ignored the trigger objects or retained their editor positions and
could export a replay that phased through the moved spike. The runtime verifier
must reject any candidate that hits it and log the object's ID and live position.

The fixture is intentionally checked in as text so object/trigger changes are
reviewable. It is not a simulator unit test: the regression specifically needs
Geometry Dash's own trigger and collision callbacks.

## `cps-70-*.lvl` (Task 3)

The cube fixture checks that the fixed 70 Hz action clock is independent of the
240 Hz physics step. The spider and swing fixtures are narrow two-sided lanes:
blind constant spam is expected to die, while search-selected actions up to the
70 CPS cap can complete. This distinguishes a cap from forced 70 CPS in modes
where every press changes player state.
