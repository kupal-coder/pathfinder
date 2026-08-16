# Path Finding

Auto-generate macros for levels using simulation! This mod uses a physics simulator under the hood to solve levels in seconds! It does not come with a bot, you will need to install one for this.

Based on camila314's Pathfinder, rebuilt with a wider simulator and a much stronger search.

# What is simulated

- Cube, ship, ball, UFO, wave, **robot**, **spider** and **swing**
- **Duals** (both players are simulated; the attempt ends if either one dies)
- Orbs including **dash orbs**, **pink dash orbs** and **spider orbs**
- Pads including the **red pad**
- Gravity, size, speed and vehicle portals
- **Teleport portals** (the classic blue/orange pair)
- Slopes, blocks, breakable blocks, hazards and sawblades

# Still not supported

- Triggers of any kind, so most 2.2 gameplay (moving objects, gravity triggers,
  time warp, target-based teleports)
- Mirror portals
- Partially rotated objects use an unrotated hitbox

If a level uses any of these, the mod now **tells you which ones** instead of
quietly producing a macro that dies.

# How To Use

1. Go to a level you want to pathfind, either in your saved or an online level.
2. Click the blue Path Finding button to start.
3. Wait for it to say **Solved!** - if it says Incomplete, the macro does not finish the level.
4. Press Export. On Android the macro is written straight to
   `Android/media/com.geode.launcher/game/macros/<level>.gdr2`; on desktop you get a file picker.
5. Import the macro in your bot and play it back.

Every exported macro is replayed from the first frame inside the simulator
before it is written, so a macro that says Solved really does reach the end.

# Click Between Frames

**Click Between Frames** and **CBF Extrapolate** are detected automatically.
Those mods place each input at its true sub-frame time, so macros that rely on
frame-exact timing can die with them enabled. When either is installed the
timings are hardened: the plan is re-replayed with every input shifted a frame
in each direction and jittered, and the timings are adjusted until it survives.
The result screen shows the resulting **timing slack**.

# Settings

* **Max CPS** - default 15, the fastest the macro is allowed to click
* **Minimum click length** - shortest press in frames, default 1
* **Harden timings** - always on when CBF is installed
