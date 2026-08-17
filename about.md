# Pathfinder

Auto-generate macros for levels using simulation! This mod uses a physics simulator under the hood to solve levels in seconds! It does not come with a bot, you will need to install one for this.

# Search and verification

Pathfinding uses camila314's original `gd-sim` rolling random-search,
partial-commit, and rollback implementation. It is fast and convenient for the
classic mechanics supported by that simulator. Every generated replay is still
checked twice in fresh Geometry Dash runtime layers and can only be exported
after two matching zero-death completions. Unsupported modern mechanics may
cause the candidate to be rejected rather than exported.

# How To Use

1. Go to a level you want to pathfind, either in your saved or an online level.
2. Click the blue Pathfinder button to start the pathfinder.
3. Export the macro into the correct folder of whichever bot you are using.
4. Import the macro and play it back!

# Report Bugs

Any simulation bugs need to be reported in the [Discord](https://discord.gg/u9m7kqyqxu)
