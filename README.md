# Pathfinder

**Pathfinder** is an automated Geometry Dash physics simulation engine and frame-exact macro solver, implemented as a high-performance **Geode Mod (C++20)** for in-game execution.

---

## 🚀 Features & Physics Support

The engine supports Geometry Dash physics with full discrete 240Hz physics stepping and 2.1 mechanics:

### 🏎️ Vehicle Physics
- **Cube**: 240Hz gravity constant (`-2793.4752` units/s²), discrete jump impulse velocities, coyote frames, ceiling hazard checks.
- **Ship**: Real-time continuous thrust, upward/downward acceleration curves, and terminal velocity clamping.
- **Ball**: Floor and ceiling gravity inversion toggles on click.
- **UFO**: Multi-impulse burst jumps in mid-air.
- **Wave**: $\pm 45^\circ$ diagonal velocity vectors matching speed multipliers, with full **D-Block** surface sliding support.
- **Robot**: Variable jump height boost physics with acceleration lasting up to 35 frames while input is held.
- **Spider (2.1)**: Vertical section raycasting across all solid surfaces and hazards for instant teleportation without transit frames.

### 🔮 Orbs & Jump Pads
- **Pads**: Yellow, Pink, Red, Blue (gravity flip), and Spider Pads (`1402`).
- **Orbs**: Yellow, Pink, Red, Blue, Green (gravity flip + jump), Black (downward force), and Spider Orbs (`1411`).
- **Dash Orbs (2.1)**: Green Dash Orbs (`1704`) and Pink Gravity Dash Orbs (`1751`) with directional vector lock and gravity suspension.

### 🛡️ 2.1 Special Collision Blocks
- **S-Block (ID: 1813)**: Stop Dash — immediately cancels active Dash Orb trajectory.
- **J-Block (ID: 1814)**: Prevent Jump Buffer — cancels held inputs to prevent unwanted buffered jumps upon landing.
- **H-Block (ID: 1815)**: Head Collision Safety — prevents fatal head collision deaths for Cube and Robot on low ceilings.
- **D-Block (ID: 1816)**: Wave Slide — allows the Wave vehicle to slide along solid block surfaces safely.

### 🌀 Portals & Speed Tiers
- **Speed Portals**: 0.5x (`251.16` u/s), 1x (`311.58` u/s), 2x (`387.42` u/s), 3x (`468.00` u/s), and 4x (`576.00` u/s).
- **Portals**: Gravity (Normal/Inverted), Size (Normal/Mini), Dual Portals, and Mirror Portals.

---

## 🛠️ Building the Geode Mod (.geode)

The repository includes pre-configured **GitHub Actions workflows** (`.github/workflows/multi-platform.yml`) using the official Geode SDK to compile cross-platform binaries.

### Automatic Build via GitHub Actions
1. Push this repository to GitHub.
2. Navigate to the **Actions** tab in your repository.
3. Select the **Build Geode Mod** workflow and click **Run workflow**.
4. Once completed, download the **Build Output** artifact ZIP containing `camila314.pathfinder.geode`.

### In-Game Installation
1. Move `camila314.pathfinder.geode` into your Geometry Dash **`geode/mods`** directory:
   - **Windows**: `Steam/steamapps/common/Geometry Dash/geode/mods/`
   - **Android / macOS**: Inside your respective Geode mod storage path.
2. Ensure you have **`geode.node-ids`** (v1.18.0+) enabled via the Geode in-game mod browser.
3. Launch Geometry Dash and open any level to access the Pathfinder solver interface.

---

## 📝 Coordinate System Notes
- In Geometry Dash runtime, internal object Y positions are offset by `+105.0` units relative to editor floor bounds.
- All simulation velocity lookups use 5-speed indexing aligned with 240Hz tick rate constants.