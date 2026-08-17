# Pathfinder

Auto-generate macros for levels using simulation! This mod uses a physics simulator under the hood to solve levels in seconds! It does not come with a bot, you will need to install one for this.

# Search and verification

Normal pathfinding uses camila314's original rolling random-search algorithm and
standalone simulator. Every generated candidate is then replayed twice through
fresh Geometry Dash `PlayLayer` instances. A replay can only be exported when
both runtime attempts complete with zero deaths and matching traces. Levels
using mechanics unsupported by the original simulator may be rejected instead
of producing an unsafe macro.

# How To Use

1. Go to a level you want to pathfind, either in your saved or an online level.
2. Click the blue Pathfinder button to start the pathfinder.
3. Export the macro into the correct folder of whichever bot you are using.
4. Import the macro and play it back!

# Report Bugs

Any simulation bugs need to be reported in the [Discord](https://discord.gg/u9m7kqyqxu)
