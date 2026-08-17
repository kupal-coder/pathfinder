# beta 25

- Load Macro: new folder button in EditLevelLayer and LevelInfoLayer that opens the mod's save folder (or Eclipse Menu's replays folder) and loads files
- Macros (.gdr2/.gdr) are parsed and shown in a popup with their info (bot, level, inputs, frames, duration)
- Verify button simulates a loaded macro against the current level and reports if it completes it or where it dies
- Save As lets you copy a loaded macro to any bot's replay folder
- Level files (raw, gzip/zlib compressed, or base64 level strings) are detected automatically - a "Level Loaded" popup lets you Pathfind the level directly
- File picker shows all files in the folder (no extension filter), and clearly reports when a picked file is neither a macro nor a level
- Fixed compile errors in the loader (missing UIBuilder include, invalid Result::unwrapErr calls)

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