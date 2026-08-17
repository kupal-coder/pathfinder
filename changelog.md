# beta 25

- Restored camila314's original rolling random-search algorithm for normal pathfinding
- Kept level checksums, 70-CPS export validation, and double runtime verification as export gates
- Search now runs against Geometry Dash's exact runtime PlayLayer and checkpoint state
- Fixed hidden PlayLayers staying frozen at 0.00% by explicitly starting gameplay
- Event-driven diversity beam search with no-input, hold, adaptive-CPS, and timed-jump templates
- Fast-path rollouts extend promising 0-70 CPS cube/ball/robot patterns up to 960 frames
- Dynamic branch count, beam width, obstacle horizon, deterministic restarts, and configurable 2-20 ms UI budget
- Runtime throughput metrics show expanded states, physics frames, updates/sec, and the next gameplay event
- Cooperative double verification with first-divergent-frame diagnostics and level checksums
- Platformer jump/left/right inputs, reverse-safe safety scoring, action pruning, shared path storage, and replay minimization
- Hidden PlayLayer lifecycle restoration on completion, cancellation, and exceptions
- Bundled 34-level in-game acceptance suite with filtering, per-test timeouts, JSON reports, and local diagnostic bundles
- Dual-player search and all runtime vehicle/object/trigger behavior, including 2.2
- Double fresh-runtime replay verification with deterministic replay seeds and trace hashes
- Mismatches log P1/P2, collision object transform, player position, and velocity and are never exported
- Oriented collision geometry for rotated solids in simulator diagnostics
- Framerate-independent 70 CPS action clock with an export-time rolling cap validator
- Adaptive candidates sample the full 0-70 CPS range; 70 is never forced
- Mode-aware scheduling adapts immediately after portals, with sparse discrete spider/swing actions
- Feature-focused manual acceptance levels and scheduler/geometry regression tests

# beta 24

- Red Orb & Red Pad
- Black Orb
- Green Orb

# beta 23

- Fixed object snapping forever
- Partial upside-down slope support
- Start Pos support
- More bug fixes

# beta 22.1 (bugfix)
- Fixed bug where player's hitbox was larger than intended

# beta 22

- Rotated object support for ship and wave
- Slope fixes
- 4x speed
- Wave fixes
- Debug button added
- Fixed bounds for vehicles in level settings

# beta 21.1 (minor fix)
- Fixed bug where hitting pads while wave caused an error

# beta 21

- Full (hopefully) wave support
- More slope fixes
- Fixed bug where ship was inaccurate in beta 20
- Other minor fixes

# beta 20

- Lots of slope fixes
- Ship hitbox fixes

# beta 19

- Fix ship accelerations
- Wave is now slightly more functional
- Slope ejections are accurate
- Everything is debug mode now

# beta 18
## Fix tons of bugs:
- Blue portal on floor would kill
- Landing on right edge of blocks did wrong x-snapping
- Small cube didn't snap on 90-60 stairs
- Priority issue when hitting block and orb on the same frame

# beta 17
- Fix bug where export doesn't show up

# beta 1-16
- Initial Release
- geode index does not allow for link transfers without updating the entire mod (and therefore pushing a new update to everyone)
- this is not a new version. it is the same version. the geode index people do not like flexibility (?) 