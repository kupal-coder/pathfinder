# Path Finding Pro

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

1. Go to a level you want to solve, either in your saved or an online level.
2. Click the blue Path Finding Pro button to start the search.
3. The macro is **saved automatically** to the mod's own macro library.
4. Open the library any time from the second button on the level screen.

Macros stay in the library until you delete them, so you no longer have to
export straight into another mod's folder just to keep a result.

# Macro Library

The library lists every macro you have made, newest first, showing how far the
route got, how long it runs and how many inputs it uses. A green percentage
means the level is fully solved; orange means the route is partial.

For each macro you can:

- **Export** - send it to your bot's replay folder (detected automatically) or
  pick any folder yourself.
- **Rename** - give it a name that means something to you.
- **Delete** - remove it for good.

There is also an **Open Folder** button if you would rather manage the files
directly.

# What the display means

- **Percentage** - how far into the level the best route reaches.
- **States** - how many distinct situations the search is currently holding.
- **Backtracking...** - the search hit a wall and is trying earlier alternatives.
  This is normal on hard sections.

If a level cannot be beaten by the simulator, the search now says so and stops
instead of running forever.

# Report Bugs

Any simulation bugs need to be reported in the [Discord](https://discord.gg/u9m7kqyqxu)
