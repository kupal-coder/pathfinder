# beta 25

- Search now runs against Geometry Dash's exact runtime PlayLayer and checkpoint state
- Dual-player search and all runtime vehicle/object/trigger behavior, including 2.2
- Runtime replay verification; mismatches log the collision object and are never exported
- Oriented collision geometry for rotated solids in simulator diagnostics
- Framerate-independent 70 CPS action clock
- Spider and swing use 70 only as a cap; constant 70 CPS pulses are limited to cube, ball, and robot
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