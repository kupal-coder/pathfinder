# Manual acceptance levels

These files are raw, uncompressed Geometry Dash level strings. Paste/import one
into a local editor level before running the named check. A task passes only if
the exported replay completes when played back without safe mode.

## `runtime-phasing.lvl` (Task 2)

The trigger graph moves group 7 into the player's route. The old standalone
world either ignored the trigger objects or retained their editor positions and
could export a replay that phased through the moved spike. The runtime verifier
must reject any candidate that hits it and log the object's ID and live position.

The fixture is intentionally checked in as text so object/trigger changes are
reviewable. It is not a simulator unit test: the regression specifically needs
Geometry Dash's own trigger and collision callbacks.
