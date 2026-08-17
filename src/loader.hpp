#pragma once

#include <Geode/Geode.hpp>

/**
 * Load a macro (.gdr2) file for the given level.
 *
 * Opens a file picker rooted at the mod's save directory (or Eclipse Menu's
 * replays folder when that mod is installed), parses the selected replay and
 * shows a popup with its info, plus the option to verify it against the level
 * with the simulator and to save a copy elsewhere.
 *
 * @param level The level the macro belongs to (used for verification).
 * @param parent The layer the popup should be added to.
 */
void openMacroLoader(GJGameLevel* level, cocos2d::CCNode* parent);
