#include <Geode/Geode.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/utils/coro.hpp>
#include <Geode/modify/EditLevelLayer.hpp>
#include <Geode/modify/LevelInfoLayer.hpp>
#include <UIBuilder.hpp>
#include "pathfinder.hpp"
#include <future>
#include <chrono>

using namespace geode::prelude;
using namespace geode::utils::file;

class PathfinderNode : public CCLayerColor {
    std::atomic_bool m_stop = false;
    std::atomic<double> m_progress = 0;
    std::future<PathfindResult> m_result;
    bool m_finalized = false;
    std::vector<uint8_t> m_macroData;
    std::string m_levelName;
    bool m_solved = false;
    // D8/D9: wall-clock bookkeeping for the progress bar and auto-stop.
    std::chrono::steady_clock::time_point m_startTime;
    std::chrono::steady_clock::time_point m_lastProgressAt;
    double m_lastProgressVal = 0.0;
    int m_maxIdleSeconds = 0;
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
        if (m_result.valid()) {
            m_result.wait();
        }
    }

    void finalize(PathfindResult result) {
        if (m_finalized) return;
        m_finalized = true;
        m_macroData = std::move(result.replay);
        m_solved = result.solved;
        if (m_solved) {
            if (auto barBg = getChildByIDRecursive("bar-bg")) {
                if (auto barFg = getChildByIDRecursive("bar-fg")) {
                    barFg->setContentSize({barBg->getContentSize().width, barFg->getContentSize().height});
                }
            }
        }
		if (!result.error.empty()) {
			Notification::create(fmt::format("Pathfinding failed: {}", result.error), NotificationIcon::Error)->show();
		} else if (!m_solved) {
			Notification::create(fmt::format("No complete solution found ({:.2f}%)", result.progress), NotificationIcon::Warning)->show();
			if (result.minMargin > 0.0f && result.minMargin < 12.0f) {
				Notification::create(
					fmt::format("Tightest safe reaction is only {:.0f} frame(s) — macro may desync", result.minMargin),
					NotificationIcon::Warning)->show();
			}
		}

		// Non-blocking heads-up (A1): the level uses triggers/movers the sim
		// can't model, so the solve is best-effort even when it succeeds.
		if (!result.warning.empty()) {
			Notification::create(fmt::format("{}", result.warning), NotificationIcon::Warning)->show();
		}

		// Solver stats (C7): surface the diagnostics so a reproducible run is
		// actually reproduce-able.
		log::info("Pathfinder finished: {} ms, {} frames simulated across {} branch(es), seed {}",
			result.wallTimeMs, result.framesSimulated, result.branchesUsed, result.seedUsed);

        if (auto stopBtn = getChildByIDRecursive("stop")) {
            stopBtn->setVisible(false);
        }
 
        auto callback = [this]() -> arc::Future<void> {
            auto saveDir = Mod::get()->getSaveDir();
            if (Loader::get()->isModLoaded("eclipse.eclipse-menu")) {
                saveDir = Loader::get()->getLoadedMod("eclipse.eclipse-menu")->getSaveDir() / "replays";
            }

            std::error_code ec;
            if (!std::filesystem::exists(saveDir, ec)) {
                std::filesystem::create_directories(saveDir, ec);
            }

            // Sanitize filename for Android and Windows filesystem safety
            std::string safeName = m_levelName.empty() ? "level" : m_levelName;
            for (char& c : safeName) {
                if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
                    c = '_';
                }
            }

            auto defaultPath = saveDir / fmt::format("{}.gdr2", safeName);

            FilePickOptions opts(
                defaultPath, {{
                std::string("Macro File"),
                std::unordered_set {std::string("gdr2")}
            }});

            auto path = co_await pick(PickMode::SaveFile, opts);
            std::filesystem::path targetPath;
            if (path.isOk() && path.unwrap().has_value()) {
                targetPath = *path.unwrap();
            } else {
                // Fallback direct save if system picker fails or is unsupported on the platform
                targetPath = defaultPath;
            }

            if (!m_macroData.empty()) {
                auto writeRes = writeBinary(targetPath, m_macroData);
                if (writeRes.isOk()) {
                    log::info("Successfully exported macro to {}", targetPath.string());
                    Notification::create(m_solved ? "Macro exported successfully!" : "Partial macro exported", m_solved ? NotificationIcon::Success : NotificationIcon::Warning)->show();
                } else {
                    log::error("Failed to write macro: {}", writeRes.unwrapErr());
                    Notification::create("Failed to write macro file", NotificationIcon::Error)->show();
                }
            } else {
                log::warn("Pathfinder macro data is empty (no inputs were recorded or level was not solved)");
                Notification::create("Warning: Macro data is empty", NotificationIcon::Warning)->show();
            }

            queueInMainThread([this] {
                removeFromParentAndCleanup(true);
            });
        };

        if (auto menu = getChildByID("menu")) {
            Build<ButtonSprite>::create("Export", "bigFont.fnt", "GJ_button_01.png")
                .intoMenuItem(async::wrapSpawn(callback))
                .scale(0.8)
                .move(0, -40)
                .parent(menu);
        }
    }

    void keyBackClicked() override  {
        m_stop = true;
        if (auto stopBtn = getChildByIDRecursive("stop"))
            stopBtn->setVisible(false);
        if (auto closeBtn = getChildByIDRecursive("close"))
            closeBtn->setVisible(false);
        if (auto label = getChildByIDRecursive("percent"))
            static_cast<CCLabelBMFont*>(label)->setString("Stopping...");
    }

    bool init(std::string const& levelName, std::string const& lvlString) {
        CCLayerColor::initWithColor({0, 0, 0, 100});
        setCascadeOpacityEnabled(true);

        m_levelName = levelName;

        m_startTime = std::chrono::steady_clock::now();
        m_lastProgressAt = m_startTime;
        m_maxIdleSeconds = static_cast<int>(Mod::get()->getSettingValue<int64_t>("max-idle-seconds"));

        // Read settings on the UI thread before spawning the solver: Geode
        // setting access isn't guaranteed thread-safe from worker threads.
        int inputOffset = static_cast<int>(Mod::get()->getSettingValue<int64_t>("input-offset"));
        int solverSeed = static_cast<int>(Mod::get()->getSettingValue<int64_t>("solver-seed"));

        m_result = std::async(std::launch::async, [lvlString, this, inputOffset, solverSeed]() {
            try {
            return pathfind(lvlString, m_stop, [this](double progress) {
                if (m_progress < progress)
                    m_progress = progress;
            }, inputOffset, solverSeed);
            } catch (std::exception& e) {
                log::error("{}", e.what());
                PathfindResult result;
                result.error = e.what();
                return result;
            }
        });

        setKeypadEnabled(true);

        Build(this).initTouch().schedule([this](float) {
            // D8: live progress — percentage, elapsed time, and a fill bar.
            double elapsedS = std::chrono::duration<double>(
                std::chrono::steady_clock::now() - m_startTime).count();
            long mins = static_cast<long>(elapsedS / 60.0);
            long secs = static_cast<long>(elapsedS) % 60;

            Build(this).intoChildRecurseID<CCLabelBMFont>("percent")
                .string(fmt::format("{:.2f}%  {:02}:{:02}", m_progress, mins, secs).c_str());

            if (auto barBg = getChildByIDRecursive("bar-bg")) {
                float barW = barBg->getContentSize().width;
                if (auto barFg = getChildByIDRecursive("bar-fg")) {
                    barFg->setContentSize({
                        barW * static_cast<float>(std::min(m_progress.load(), 100.0) / 100.0),
                        barFg->getContentSize().height
                    });
                }
            }

            // D9: auto-stop when no new progress for m_maxIdleSeconds.
            auto now = std::chrono::steady_clock::now();
            if (m_progress > m_lastProgressVal) {
                m_lastProgressVal = m_progress;
                m_lastProgressAt = now;
            } else if (m_maxIdleSeconds > 0 && !m_finalized && m_result.valid() &&
                       std::chrono::duration_cast<std::chrono::seconds>(now - m_lastProgressAt).count() >= m_maxIdleSeconds) {
                m_stop = true;
            }

            if (m_result.valid() && m_result.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                finalize(m_result.get());
            }
        });

        auto handle = [this](CCMenuItemSpriteExtra* it) {
            m_stop = true;

            if (it->getID() == "stop") {
                if (auto stopBtn = getChildByIDRecursive("stop"))
                    stopBtn->setVisible(false);
                if (auto closeBtn = getChildByIDRecursive("close"))
                    closeBtn->setVisible(false);
                if (auto label = getChildByIDRecursive("percent"))
                    static_cast<CCLabelBMFont*>(label)->setString("Stopping...");
            } else {
                if (auto closeBtn = getChildByIDRecursive("close"))
                    closeBtn->setVisible(false);
                if (auto label = getChildByIDRecursive("percent"))
                    static_cast<CCLabelBMFont*>(label)->setString("Stopping...");
            }
        };

        auto menu = Build<CCMenu>::create().parent(this).id("menu").children(
            Build<CCScale9Sprite>::create("GJ_square02.png")
                .contentSize(250, 140),
            Build<CCLabelBMFont>::create("Pathfinding Pro Max", "bigFont.fnt")
                .move(0, 50)
                .scale(0.8),
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

        // D8: progress fill bar, kept below the percent label. Anchored to the
        // label itself (rather than the layer) so it follows the popup layout
        // no matter how the UI gets repositioned.
        if (auto pctLabel = getChildByIDRecursive("percent")) {
            Build<CCLayerColor>::create({0, 0, 0, 150})
                .contentSize({150, 6})
                .id("bar-bg")
                .move(0, -18)
                .parent(pctLabel);
            Build<CCLayerColor>::create({70, 200, 70, 255})
                .contentSize({0, 6})
                .id("bar-fg")
                .move(0, -18)
                .parent(pctLabel);
        }

        return true;
    }

};

class $modify(PathfinderEditLevelLayer, EditLevelLayer) {
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

class $modify(PathfinderLevelInfoLayer, LevelInfoLayer) {
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
