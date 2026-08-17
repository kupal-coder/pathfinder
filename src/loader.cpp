#include <Geode/Geode.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/utils/coro.hpp>
#include <Geode/utils/file.hpp>
#include <UIBuilder.hpp>
#include <gdr/gdr.hpp>
#include <Level.hpp>
#include "loader.hpp"
#include "PathfinderNode.hpp"

#include <algorithm>
#include <filesystem>
#include <optional>

using namespace geode::prelude;
using namespace geode::utils::file;

namespace {

/// Replay type matching the .gdr2 format the mod exports.
class LoadedReplay : public gdr::Replay<LoadedReplay, gdr::Input<"">> {
 public:
    LoadedReplay() : Replay("Pathfinder", 1) {}
};

/// Directory the mod saves (and can load) macros from. Mirrors the export
/// behaviour: when Eclipse Menu is installed, its replays folder is used.
std::filesystem::path macroSaveDir() {
    auto dir = Mod::get()->getSaveDir();
    if (Loader::get()->isModLoaded("eclipse.eclipse-menu")) {
        dir = Loader::get()->getLoadedMod("eclipse.eclipse-menu")->getSaveDir() / "replays";
    }
    return dir;
}

std::string shorten(std::string const& str, size_t maxLen) {
    if (str.size() <= maxLen) return str;
    return str.substr(0, maxLen - 3) + "...";
}

// ---------------------------------------------------------------- macro verify

struct VerifyResult {
    bool passed = false;
    std::string message;
};

/// Simulate a macro against a level string using gd-sim.
/// Only player-1 jump inputs are simulated (gd-sim has no platformer support).
VerifyResult verifyMacro(std::string const& lvlString, std::vector<gdr::Input<>> const& inputs) {
    std::vector<gdr::Input<>> jumpInputs;
    for (auto const& i : inputs) {
        if (!i.player2 && (i.button == 0 || i.button == 1))
            jumpInputs.push_back(i);
    }
    std::sort(jumpInputs.begin(), jumpInputs.end(), [](auto const& a, auto const& b) {
        return a.frame < b.frame;
    });

    if (jumpInputs.empty()) {
        return {false, "Macro has no player-1 jump inputs to simulate"};
    }

    Level lvl(lvlString);
    lvl.debug = false;

    if (lvl.length <= 0) {
        return {false, "Level is empty - nothing to verify against"};
    }

    // Safety cap: 200k frames is ~14 minutes at 240 fps, way beyond any real macro.
    constexpr uint64_t maxSimFrames = 200'000;

    bool press = false;
    size_t idx = 0;
    for (uint64_t f = 1; f <= maxSimFrames; ++f) {
        // Apply every input recorded at or before this frame, just like
        // Pathfinder's own search does (input is checked before the frame runs).
        while (idx < jumpInputs.size() && jumpInputs[idx].frame <= f) {
            press = jumpInputs[idx].down;
            ++idx;
        }

        auto& state = lvl.runFrame(press);

        if (state.dead) {
            return {false, fmt::format(
                "Macro failed at frame {} ({:.1f}% in)",
                lvl.currentFrame(), std::min(state.pos.x / lvl.length * 100, 100.0f)
            )};
        }
        if (state.pos.x >= lvl.length) {
            return {true, fmt::format(
                "Macro completes the level at frame {}", lvl.currentFrame()
            )};
        }
    }

    auto& state = lvl.latestState();
    return {false, fmt::format(
        "Macro did not complete the level (only reached {:.1f}%)",
        std::min(state.pos.x / lvl.length * 100, 100.0f)
    )};
}

// ------------------------------------------------------------- level decoding

std::string base64Decode(std::string const& in) {
    static constexpr char const* chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int table[256];
    std::fill(std::begin(table), std::end(table), -1);
    for (int i = 0; i < 64; ++i)
        table[static_cast<unsigned char>(chars[i])] = i;

    std::string out;
    int val = 0, valb = -8;
    for (unsigned char c : in) {
        if (table[c] == -1) {
            if (c == '=') break;
            continue;
        }
        val = (val << 6) + table[c];
        valb += 6;
        if (valb >= 0) {
            out.push_back(static_cast<char>((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out;
}

/// Try to interpret a file's bytes as a level string: raw, gzip, zlib,
/// base64, and combinations. Returns the level string if gd-sim can parse it.
std::optional<std::string> decodeLevelString(std::vector<uint8_t> const& bytes) {
    std::string raw(bytes.begin(), bytes.end());

    auto looksLikeLevel = [](std::string const& s) -> bool {
        if (s.empty() || s.size() < 8) return false;
        try {
            Level lvl(s);
            return lvl.objectCount > 0;
        } catch (...) {
            return false;
        }
    };

    if (looksLikeLevel(raw)) return raw;

    if (auto d = ZipUtils::decompressString(raw.c_str(), true, 0); looksLikeLevel(d)) return d;
    if (auto d = ZipUtils::decompressString(raw.c_str(), false, 0); looksLikeLevel(d)) return d;

    auto b64 = base64Decode(raw);
    if (looksLikeLevel(b64)) return b64;
    if (auto d = ZipUtils::decompressString(b64.c_str(), true, 0); looksLikeLevel(d)) return d;
    if (auto d = ZipUtils::decompressString(b64.c_str(), false, 0); looksLikeLevel(d)) return d;

    return std::nullopt;
}

// --------------------------------------------------------------------- popups

/// Popup shown after a macro is loaded: displays its info and offers
/// verification via the simulator, plus saving a copy elsewhere.
class MacroInfoPopup : public CCLayerColor {
    std::string m_fileName;
    std::string m_levelString;
    LoadedReplay m_replay;
    std::vector<uint8_t> m_rawData;
    CCLabelBMFont* m_status = nullptr;

 public:
    static MacroInfoPopup* create(
        std::string const& fileName,
        LoadedReplay replay,
        std::vector<uint8_t> rawData,
        std::string const& levelString
    ) {
        auto ret = new MacroInfoPopup();
        if (ret && ret->init(fileName, std::move(replay), std::move(rawData), levelString)) {
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }

    void keyBackClicked() override {
        CCLayer::keyBackClicked();
        removeFromParentAndCleanup(true);
    }

    bool init(
        std::string const& fileName,
        LoadedReplay replay,
        std::vector<uint8_t> rawData,
        std::string const& levelString
    ) {
        CCLayerColor::initWithColor({0, 0, 0, 100});
        setCascadeOpacityEnabled(true);
        setKeypadEnabled(true);

        m_fileName = fileName;
        m_replay = std::move(replay);
        m_rawData = std::move(rawData);
        m_levelString = levelString;

        uint64_t maxFrame = 0;
        size_t skippedInputs = 0;
        for (auto const& i : m_replay.inputs) {
            maxFrame = std::max<uint64_t>(maxFrame, i.frame);
            if (i.player2 || (i.button != 0 && i.button != 1))
                ++skippedInputs;
        }
        double fps = m_replay.framerate > 0 ? m_replay.framerate : 240.0;

        auto botName = m_replay.botInfo.name.empty() ? "Unknown" : m_replay.botInfo.name;
        auto lvlName = m_replay.levelInfo.name.empty() ? "Unknown" : m_replay.levelInfo.name;

        auto info = fmt::format(
            "{}\nBot: {} (v{})\nLevel: {}{}\nInputs: {} | Frames: {}\nDuration: {:.2f}s @ {:.0f} FPS{}",
            shorten(m_fileName, 30),
            shorten(botName, 20),
            m_replay.botInfo.version,
            shorten(lvlName, 20),
            m_replay.levelInfo.id != 0 ? fmt::format(" ({})", m_replay.levelInfo.id) : "",
            m_replay.inputs.size(),
            maxFrame,
            maxFrame / fps,
            fps,
            skippedInputs ? fmt::format(" | {} P2/non-jump skipped", skippedInputs) : ""
        );

        Build<CCMenu>::create().parent(this).id("menu").children(
            Build<CCScale9Sprite>::create("GJ_square02.png")
                .contentSize(340, 250),
            Build<CCLabelBMFont>::create("Macro Loaded", "bigFont.fnt")
                .move(0, 100)
                .scale(0.6),
            Build<CCLabelBMFont>::create(info.c_str(), "chatFont.fnt")
                .id("info")
                .move(0, 40)
                .scale(0.55),
            Build<CCLabelBMFont>::create("Press Verify to check this macro", "chatFont.fnt")
                .id("status")
                .move(0, -40)
                .scale(0.55),
            Build<ButtonSprite>::create("Verify", "bigFont.fnt", "GJ_button_01.png")
                .intoMenuItem([this]() { onVerify(); })
                .id("verify")
                .scale(0.65)
                .move(-70, -90),
            Build<ButtonSprite>::create("Save As", "bigFont.fnt", "GJ_button_01.png")
                .intoMenuItem(async::wrapSpawn([this](this auto self) -> arc::Future<void> {
                    FilePickOptions opts(macroSaveDir() / m_fileName, {{
                        std::string("Macro File"),
                        std::unordered_set{std::string("gdr2"), std::string("gdr")}
                    }});
                    if (auto path = co_await pick(PickMode::SaveFile, opts); path.isOk() && path.unwrap().has_value()) {
                        auto res = writeBinary(*path.unwrap(), m_rawData);
                        if (res.isOk()) {
                            m_status->setString("Saved!");
                            m_status->setColor(ccc3(0, 255, 120));
                        } else {
                            m_status->setString(fmt::format("Save failed: {}", std::move(res).unwrapErr()).c_str());
                            m_status->setColor(ccc3(255, 80, 80));
                        }
                    }
                }))
                .id("save-as")
                .scale(0.65)
                .move(70, -90),
            Build<CCSprite>::createSpriteName("GJ_closeBtn_001.png")
                .intoMenuItem([this]() { removeFromParentAndCleanup(true); })
                .id("close")
                .move(-155, 110)
                .scale(0.8)
        );

        m_status = static_cast<CCLabelBMFont*>(getChildByIDRecursive("status"));
        return true;
    }

    void onVerify() {
        if (m_levelString.empty()) {
            m_status->setString("Level data not loaded - cannot verify");
            m_status->setColor(ccc3(255, 170, 60));
            return;
        }
        m_status->setString("Verifying...");
        m_status->setColor(ccc3(255, 255, 255));
        try {
            auto result = verifyMacro(m_levelString, m_replay.inputs);
            m_status->setString(result.message.c_str());
            m_status->setColor(result.passed ? ccc3(0, 255, 120) : ccc3(255, 80, 80));
        } catch (std::exception const& e) {
            m_status->setString(fmt::format("Simulation error: {}", e.what()).c_str());
            m_status->setColor(ccc3(255, 80, 80));
        }
    }
};

/// Popup shown after a level/save file is loaded: shows its stats and lets the
/// user pathfind it (or save the level string elsewhere).
class LevelInfoPopup : public CCLayerColor {
    std::string m_fileName;
    std::string m_levelString;

 public:
    static LevelInfoPopup* create(std::string const& fileName, std::string const& levelString) {
        auto ret = new LevelInfoPopup();
        if (ret && ret->init(fileName, levelString)) {
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }

    void keyBackClicked() override {
        CCLayer::keyBackClicked();
        removeFromParentAndCleanup(true);
    }

    bool init(std::string const& fileName, std::string const& levelString) {
        CCLayerColor::initWithColor({0, 0, 0, 100});
        setCascadeOpacityEnabled(true);
        setKeypadEnabled(true);

        m_fileName = fileName;
        m_levelString = levelString;

        size_t objectCount = 0;
        float length = 0;
        try {
            Level lvl(levelString);
            objectCount = lvl.objectCount;
            length = lvl.length;
        } catch (...) {
        }

        auto info = fmt::format(
            "{}\nObjects: {}\nLength: {:.0f}",
            shorten(m_fileName, 30),
            objectCount,
            length
        );

        Build<CCMenu>::create().parent(this).id("menu").children(
            Build<CCScale9Sprite>::create("GJ_square02.png")
                .contentSize(340, 200),
            Build<CCLabelBMFont>::create("Level Loaded", "bigFont.fnt")
                .move(0, 80)
                .scale(0.6),
            Build<CCLabelBMFont>::create(info.c_str(), "chatFont.fnt")
                .id("info")
                .move(0, 25)
                .scale(0.55),
            Build<ButtonSprite>::create("Pathfind", "bigFont.fnt", "GJ_button_01.png")
                .intoMenuItem([this]() { onPathfind(); })
                .id("pathfind")
                .scale(0.65)
                .move(-70, -60),
            Build<ButtonSprite>::create("Save As", "bigFont.fnt", "GJ_button_01.png")
                .intoMenuItem(async::wrapSpawn([this](this auto self) -> arc::Future<void> {
                    FilePickOptions opts(macroSaveDir() / (m_fileName + ".txt"), {{
                        std::string("Level File"),
                        std::unordered_set{std::string("txt"), std::string("dat")}
                    }});
                    if (auto path = co_await pick(PickMode::SaveFile, opts); path.isOk() && path.unwrap().has_value()) {
                        (void)writeString(*path.unwrap(), m_levelString);
                        removeFromParentAndCleanup(true);
                    }
                }))
                .id("save-as")
                .scale(0.65)
                .move(70, -60),
            Build<CCSprite>::createSpriteName("GJ_closeBtn_001.png")
                .intoMenuItem([this]() { removeFromParentAndCleanup(true); })
                .id("close")
                .move(-155, 90)
                .scale(0.8)
        );

        return true;
    }

    void onPathfind() {
        auto parent = getParent();
        Build<PathfinderNode>::create(m_fileName, m_levelString).parent(parent).zOrder(100);
        removeFromParentAndCleanup(true);
    }
};

}  // namespace

void openMacroLoader(GJGameLevel* level, cocos2d::CCNode* parent) {
    if (!level || !parent) return;

    auto callback = [level, parent](this auto self) -> arc::Future<void> {
        auto dir = macroSaveDir();

        if (!std::filesystem::exists(dir)) {
            std::error_code ec;
            std::filesystem::create_directories(dir, ec);
        }

        FilePickOptions opts(dir);  // no extension filter: show every file in the folder

        if (auto val = co_await pick(PickMode::OpenFile, opts); val.isOk() && val.unwrap().has_value()) {
            auto path = *val.unwrap();

            auto data = readBinary(path);
            if (data.isErr()) {
                FLAlertLayer::create(
                    "Load Macro",
                    fmt::format("Could not read file: {}", std::move(data).unwrapErr()),
                    "OK"
                )->show();
                co_return;
            }

            auto bytes = data.unwrap();

            // Try as a macro first
            auto replay = LoadedReplay::importData(bytes);
            if (replay.isOk()) {
                auto lvlString = ZipUtils::decompressString(level->m_levelString, true, 0);
                Build<MacroInfoPopup>::create(
                    path.filename().string(),
                    std::move(replay).unwrap(),
                    bytes,
                    lvlString
                ).parent(parent).zOrder(100);
                co_return;
            }
            auto macroErr = std::move(replay).unwrapErr();

            // Not a macro - try as a level/save file
            if (auto lvlString = decodeLevelString(bytes); lvlString.has_value()) {
                Build<LevelInfoPopup>::create(
                    path.filename().string(),
                    *lvlString
                ).parent(parent).zOrder(100);
                co_return;
            }

            FLAlertLayer::create(
                "Load Macro",
                fmt::format(
                    "\"{}\" is neither a macro nor a level file.\n{}",
                    path.filename().string(),
                    macroErr
                ),
                "OK"
            )->show();
        }
    };

    async::wrapSpawn(callback)();
}
