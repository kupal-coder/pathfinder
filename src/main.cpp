#include <Geode/Geode.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/utils/coro.hpp>
#include <Geode/modify/EditLevelLayer.hpp>
#include <Geode/modify/LevelInfoLayer.hpp>
#include <UIBuilder.hpp>
#include "pathfinder.hpp"
#include <future>
#include <Geode/loader/Dirs.hpp>
#include <filesystem>
#ifdef GEODE_IS_ANDROID
#include <Geode/ui/Notification.hpp>
#include <fstream>
#endif

using namespace geode::prelude;
using namespace geode::utils::file;

/**
 * Click Between Frames applies inputs at their true sub-frame time instead of
 * snapping them to a physics frame, and CBF Extrapolate builds on it. A macro
 * whose timings only work frame-exact can therefore die with them enabled, so
 * the search hardens its timings against a frame of slop when either is loaded.
 */
/**
 * Where a bot will actually look for the macro.
 *
 * Eclipse's replay browser only lists files inside its own mod folder, so that
 * wins when Eclipse is present - checked by path as well as by load state, so a
 * temporarily disabled Eclipse still gets its macros. Otherwise fall back to the
 * shared `game/macros` folder on Android, or this mod's save folder on desktop.
 */
static std::filesystem::path macroFolder() {
    if (auto eclipse = Loader::get()->getLoadedMod("eclipse.eclipse-menu"))
        return eclipse->getSaveDir() / "replays";

    auto eclipseReplays = dirs::getModsSaveDir() / "eclipse.eclipse-menu" / "replays";
    std::error_code ec;
    if (std::filesystem::is_directory(eclipseReplays, ec))
        return eclipseReplays;

#ifdef GEODE_IS_ANDROID
    return dirs::getGameDir() / "macros";
#else
    return Mod::get()->getSaveDir();
#endif
}

static bool clickBetweenFramesLoaded() {
    auto loader = Loader::get();
    return loader->isModLoaded("syzzi.click_between_frames")
        || loader->isModLoaded("square3ang.cbfextrapolate");
}

class PathfinderNode : public CCLayerColor {
    std::atomic_bool m_stop = false;
    std::atomic<double> m_progress = 0;
    std::future<PathfindResult> m_result;
    std::string m_levelName;
    bool m_done = false;
    bool m_cbf = false;
public:
    static PathfinderNode* create(std::string const& levelName, std::string const& lvlString) {
        auto node = new PathfinderNode();
        if (node && node->init(levelName, lvlString)) {
            node->autorelease();
            return node;
        }
        CC_SAFE_DELETE(node);
        return nullptr;
    }

    ~PathfinderNode() {
        m_stop = true;
        if (m_result.valid())
            m_result.get();
    }

    void finalize(PathfindResult result) {
        if (m_done)
            return;
        m_done = true;

        if (auto stopBtn = getChildByIDRecursive("stop"))
            stopBtn->setVisible(false);

        // Say plainly whether this macro actually finishes the level. A run that
        // stopped at 40% is still exportable, but it must not look like a win.
        Build(this).intoChildRecurseID<CCLabelBMFont>("title")
            .string(result.completed ? "Solved!" : "Incomplete");

        Build(this).intoChildRecurseID<CCLabelBMFont>("percent")
            .string(fmt::format("{:.1f}% - {} clicks - {:.0f} cps",
                result.progress, result.clicks, result.peakCps).c_str())
            .scale(0.7f);

        log::info("{:.1f}% - {} clicks - peak {:.0f} cps - {:.0f}% timing slack",
            result.progress, result.clicks, result.peakCps, result.robustness);

        // Unsupported objects are the most useful thing to surface, then how
        // much timing slack the macro has.
        std::string detail;
        if (!result.warnings.empty()) {
            log::warn("level uses mechanics the simulator cannot model:");
            for (auto const& w : result.warnings)
                log::warn("  - {}", w);

            detail = fmt::format("Unsupported: {}", result.warnings.front());
        } else if (result.completed) {
            detail = fmt::format("{:.0f}% timing slack{}",
                result.robustness, m_cbf ? " (CBF safe)" : "");
        }

        if (!detail.empty()) {
            Build(this).intoChildRecurseID<CCLabelBMFont>("warning")
                .string(detail.c_str())
                .scale(0.32f);
        }

        auto macro = result.macro;

        auto callback = [this, macro](this auto self) -> arc::Future<void> {
            auto saveDir = macroFolder();

            std::error_code ec;
            std::filesystem::create_directories(saveDir, ec);

            auto fileName = m_levelName.empty() ? std::string("pathfinder") : m_levelName;
            for (size_t i = 0; (i = fileName.find_first_of("/\\:*?\"<>|", i)) != std::string::npos; ++i) {
                fileName[i] = '_';
            }

            FilePickOptions opts(
                saveDir / (fileName + ".gdr2"), {{
                std::string("Macro File"),
                std::unordered_set {std::string("gdr2")}
            }});

#ifdef GEODE_IS_ANDROID
            // Android scoped storage makes the system file picker unusable here
            // (camila314/pathfinder#10, geode-sdk/geode#1287): it returns a SAF
            // content:// uri that resolves to "Failed to get file." and leaves a
            // 0-byte macro behind. Skip the picker and write the file ourselves.
            (void)opts;

            auto outPath = saveDir / (fileName + ".gdr2");

            bool wrote = false;
            if (!macro.empty()) {
                std::ofstream out(outPath, std::ios::binary | std::ios::trunc);
                if (out.is_open()) {
                    out.write(reinterpret_cast<char const*>(macro.data()), static_cast<std::streamsize>(macro.size()));
                    out.flush();
                    wrote = out.good();
                }
            }

            queueInMainThread([this, outPath, wrote] {
                if (wrote) {
                    Notification::create(
                        fmt::format("Saved to {}", outPath.string()),
                        NotificationIcon::Success, NOTIFICATION_LONG_TIME
                    )->show();
                    removeFromParentAndCleanup(true);
                } else {
                    Notification::create(
                        fmt::format("Failed to write {}", outPath.string()),
                        NotificationIcon::Error, NOTIFICATION_LONG_TIME
                    )->show();
                }
            });
            co_return;
#else
            if (auto path = co_await pick(PickMode::SaveFile, opts); path.isOk() && path.unwrap().has_value()) {
                (void)writeBinary(*path.unwrap(), macro);
                queueInMainThread([this] {
                    removeFromParentAndCleanup(true);
                });
            }
#endif
        };

        char const* exportLabel = result.completed ? "Export" : "Export anyway";

        Build<ButtonSprite>::create(exportLabel, "bigFont.fnt",
                result.completed ? "GJ_button_01.png" : "GJ_button_04.png")
            .intoMenuItem(async::wrapSpawn(callback))
            .scale(0.8)
            .move(0, -40)
            .parent(getChildByID("menu"));
    }

    void keyBackClicked() override  {
        m_stop = true;
        CCLayer::keyBackClicked();
        removeFromParentAndCleanup(true);
    }

    bool init(std::string const& levelName, std::string const& lvlString) {
        CCLayerColor::initWithColor({0, 0, 0, 100});
        setCascadeOpacityEnabled(true);

        m_levelName = levelName;

        m_cbf = clickBetweenFramesLoaded();

        PathfindOptions options;
        options.maxCps = (int)Mod::get()->getSettingValue<int64_t>("max-cps");
        options.minHoldFrames = (int)Mod::get()->getSettingValue<int64_t>("min-hold");
        options.harden = Mod::get()->getSettingValue<bool>("harden-timings");

        // Hardening is what makes a macro survive CBF, so never skip it there.
        if (m_cbf && !options.harden) {
            log::info("Click Between Frames is loaded, hardening timings anyway");
            options.harden = true;
        }

        log::info("searching with max {} cps, min hold {} frames, harden {}",
            options.maxCps, options.minHoldFrames, options.harden);

        m_result = std::async(std::launch::async, [lvlString, levelName, options, this]() -> PathfindResult {
            try {
                return pathfind(lvlString, m_stop, [this](double progress) {
                    if (m_progress < progress)
                        m_progress = progress;
                }, levelName, options);
            } catch (std::exception& e) {
                log::error("{}", e.what());
                PathfindResult failed;
                failed.warnings.push_back("the simulator hit an internal error");
                return failed;
            }
        });

        setKeypadEnabled(true);

        Build(this).initTouch().schedule([this](float) {
                // Once finalize() has run the labels show the result, so stop
                // overwriting them with the live progress.
                if (m_done)
                    return;

                Build(this).intoChildRecurseID<CCLabelBMFont>("percent")
                    .string(fmt::format("{:.2f}%", m_progress).c_str());

                if (m_result.valid() && m_result.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                    finalize(m_result.get());
                }
        });

        auto handle = [this](CCMenuItemSpriteExtra* it) {
            m_stop = true;

            if (it->getID() == "stop") {
                if (m_result.valid())
                    finalize(m_result.get());
            } else {
                removeFromParentAndCleanup(true);
            }
        };

        auto menu = Build<CCMenu>::create().parent(this).id("menu").children(
            Build<CCScale9Sprite>::create("GJ_square02.png")
                .contentSize(250, 140),
            Build<CCLabelBMFont>::create("Pathfinding...", "bigFont.fnt")
                .id("title")
                .move(0, 50)
                .scale(0.8),
            Build<CCLabelBMFont>::create("", "chatFont.fnt")
                .id("warning")
                .move(0, -12)
                .scale(0.32f),
            Build<CCLabelBMFont>::create("0.00", "chatFont.fnt")
                .id("percent")
                .move(0, 10),
            Build<ButtonSprite>::create("Stop", "bigFont.fnt", "GJ_button_04.png")
                .scale(0.8)
                .intoMenuItem(handle)
                .id("stop")
                .move(0, -40),
            Build<CCSprite>::createSpriteName("GJ_closeBtn_001.png")
                .intoMenuItem(handle)
                .id("close")
                .move(-125, 70)
                .scale(0.8)
        );

        /*    .intoNewChild(CCMenu::create())
                .id("menu")
                .intoNewChild(CCScale9Sprite::create("GJ_square04.png"))
                    .contentSize(250, 140)
                .intoNewSibling(CCLabelBMFont::create("Pathfinding...", "bigFont.fnt"))
                    .move(0, 50)
                    .scale(0.8)
                .intoNewSibling(CCLabelBMFont::create("0.00", "chatFont.fnt"))
                    .id("percent")
                    .move(0, 10)
                .intoNewSibling(ButtonSprite::create("Stop", "bigFont.fnt", "GJ_button_04.png"))
                    .intoMenuItem([this]() {
                        m_stop = true;
                        finalize(m_result.get());
                    })
                    .scale(0.8)
                    .id("cancel")
                    .move(0, -40)
                .intoNewSibling(CCSprite::createWithSpriteFrameName("GJ_closeBtn_001.png"))
                    .intoMenuItem([this]() {
                        m_stop = true;
                        this->removeFromParentAndCleanup(true);
                    })
                    .move(-125, 70)
                    .scale(0.8);*/
        ;

        return true;
    }

};

class $modify(EditLevelLayer) {
    bool init(GJGameLevel* p0) {
        EditLevelLayer::init(p0);

        auto btn = Build<BasedButtonSprite>::create(
            CCSprite::create("pathfinder.png"_spr),
            BaseType::Circle,
            4,
            3
        ).scale(0.8);

        btn->setTopRelativeScale(1.4);

        btn.intoMenuItem([this]() {
                auto lvlString = ZipUtils::decompressString(m_level->m_levelString, true, 0);
                Build<PathfinderNode>::create(m_level->m_levelName, lvlString).parent(this).zOrder(100);
        }).id("pathfinder-button")
          .intoNewParent(CCMenu::create())
          .parent(this)
          .id("pathfinder-menu")
          .matchPos(getChildByIDRecursive("delete-button"))
          .move(-45, 0);


        return true;
    }
};

class $modify(LevelInfoLayer) {
    bool init(GJGameLevel* level, bool challenge) {
        LevelInfoLayer::init(level, challenge);

        auto btn = Build<BasedButtonSprite>::create(
            CCSprite::create("pathfinder.png"_spr),
            BaseType::Circle,
            4,
            3
        ).scale(0.8);

        btn->setTopRelativeScale(1.4);

        btn.intoMenuItem([this]() {
                auto lvlString = ZipUtils::decompressString(m_level->m_levelString, true, 0);
                Build<PathfinderNode>::create(m_level->m_levelName, lvlString).parent(this).zOrder(100);
        }).id("pathfinder-button")
          .parent(getChildByID("other-menu"))
          .matchPos(getChildByIDRecursive("list-button"))
          .move(0, 45);

        return true;
    }
};
