# Pathfinder

Auto-generate macros for levels using simulation! This mod uses a physics simulator under the hood to solve levels in seconds! It does not come with a bot, you will need to install one for this.

# None of these objects are supported
- Duals
- Upside-down Slopes
- Partially Rotated Objects for Cube, UFO, Ball
- Robot Mode
- Spider Mode
- Swing Mode
- Any Non-Visual Triggers
- Dash Orbs
- Teleport Portals
- Anything from 2.2
- Modifier blocks (D-block J-block etc)

# How To Use

1. Go to a level you want to pathfind, either in your saved or an online level.
2. Click the blue Pathfinder button to start the pathfinder.
3. Export the macro into the correct folder of whichever bot you are using.
4. Import the macro and play it back!

# Load Macros

To load (and verify) a saved macro instead of generating a new one, click the
folder button next to the Pathfinder button. The file picker opens in the mod's
save folder (or Eclipse Menu's replays folder when installed) and shows every
file in it. After loading a file you can:

- **Macro (.gdr2/.gdr)** - view its info (bot, level, inputs, frames, duration),
  click **Verify** to simulate it against the current level, or **Save As** to
  copy it into a bot's replay folder
- **Level/save file** (raw, gzip/zlib compressed, or base64 level string) - the
  popup shows its object count and length, and you can **Pathfind** it directly

# Report Bugs

Any simulation bugs need to be reported in the [Discord](https://discord.gg/u9m7kqyqxu)
