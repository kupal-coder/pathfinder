# Pathfinder

Auto-generate macros for levels using simulation! This mod uses a physics simulator under the hood to solve levels in seconds! It does not come with a bot, you will need to install one for this.

# Search and verification

Pathfinding keeps camila314's original rolling random-search, partial-commit,
and rollback strategy, but candidates are stepped through exact Geometry Dash
`PlayLayer` checkpoints. This lets the same search strategy see dual players,
all runtime vehicles, triggers, moved/toggled objects, portals, 2.2 mechanics,
and modifier state without an object-ID behavior table. Every generated replay
is then checked twice in fresh runtime layers and can only be exported after two
matching zero-death completions.

# How To Use

1. Go to a level you want to pathfind, either in your saved or an online level.
2. Click the blue Pathfinder button to start the pathfinder.
3. Export the macro into the correct folder of whichever bot you are using.
4. Import the macro and play it back!

# Report Bugs

Any simulation bugs need to be reported in the [Discord](https://discord.gg/u9m7kqyqxu)
