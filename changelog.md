# v1.2.0

## Built-in macro library
- Solved macros are now **saved automatically** into the mod's own library. No
  more being dropped into a file picker aimed at another mod's folder just to
  keep a result.
- New library browser, opened from the level screen, listing every macro
  newest-first with its completion percentage, duration and input count.
  Green means solved, orange means partial.
- Each macro can be exported, renamed or deleted. Export offers a one-tap send
  to a detected bot's replay folder, or any folder you choose.
- Saved macros carry proper metadata (level name, id, duration, progress), so
  the library shows real information instead of guessing from the filename.
- Saving the same level twice no longer overwrites the earlier attempt; the new
  file is suffixed instead.
- Requires Geode 5.9.0.

# v1.1.0

Renamed to **Path Finding Pro**.

## New search engine
- Replaced the random-input search with a parallel, deduplicated best-first
  search. It backtracks properly instead of re-rolling random inputs, and uses
  every core instead of one.
- The search now recognises when a level cannot be beaten and stops, rather
  than grinding forever on an impossible section.
- Levels needing a mix of orbs, pads, portals and vehicle changes are solved
  in well under a second in testing.

## Fixes
- Macros no longer break on levels longer than ~4.5 minutes. Input frames were
  stored in 16 bits and silently wrapped past frame 65535, so inputs landed at
  the wrong place and did nothing.
- Levels with unusual or truncated object data no longer abort the whole run
  and hand back an empty macro with no explanation.
- Fixed a crash on levels containing no recognised objects.
- Fixed the out-of-bounds check comparing a Y coordinate against level length,
  so it never worked as intended.
- Fixed uninitialised player fields that could make runs non-reproducible.
- Fixed a corrupt vehicle id running off the end of a switch.
- Blocks at the very start of a level are no longer collision-tested twice.

## Performance
- Player history is now a bounded window rather than every frame ever
  simulated: 6 KB instead of ~118 MB on a long level.
- Branching during search costs ~0.34us, down from ~911us.
- Removed the two heap allocations that happened on every simulated frame.

## Project
- Added golden-file physics regression tests, search correctness tests and a
  benchmark gauntlet, all runnable without the game.
- CI now runs on every push, including an AddressSanitizer/UBSan job.

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