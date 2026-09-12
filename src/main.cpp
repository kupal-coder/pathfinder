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
        log::info("Pathfinder finished: {} ms, {} frames simulated across {} branch(es), seed {}, replay {} bytes / {} inputs, sim coverage {}/{} (top unmapped: {})",
            result.wallTimeMs, result.framesSimulated, result.branchesUsed, result.seedUsed, result.replay.size(), result.inputsRecorded,
            result.objectsModelled, result.objectsModelled + result.objectsIgnored,
            result.topIgnoredIds.empty() ? "none" : result.topIgnoredIds);

        if (auto stopBtn = getChildByIDRecursive("stop")) {
            stopBtn->setVisible(false);
        }
 
        auto callback = [this]() -> arc::Future<void> {
            // Fail fast before the file picker: exporting nothing leaves an
            // orphan 0-byte entry behind on some platforms, because the save
            // dialog creates the file up front.
            if (m_macroData.empty()) {
                log::warn("Pathfinder macro data is empty (no inputs were recorded or level was not solved)");
                Notification::create("Nothing to export: macro data is empty", NotificationIcon::Warning)->show();
                co_return;
            }

            auto saveDir = Mod::get()->getSaveDir();
            std::string saveTarget = "Pathfinder";
            // Save where the macro will actually be played back: XDBot reads
            // its own macros folder and Eclipse its replays dir. Defaulting
            // into the consumer's folder avoids round-tripping the bytes
            // through the OS save picker on Android, which can leave a 0-byte
            // entry behind while reporting success.
            if (auto xdbot = Loader::get()->getLoadedMod("zilko.xdbot")) {
                std::filesystem::path xdbotDir;
                try {
                    xdbotDir = xdbot->getSettingValue<std::filesystem::path>("macros_folder");
                } catch (...) {}
                saveDir = xdbotDir.empty() ? xdbot->getSaveDir() / "macros" : xdbotDir;
                saveTarget = "XDBot";
            } else if (auto eclipse = Loader::get()->getLoadedMod("eclipse.eclipse-menu")) {
                saveDir = eclipse->getSaveDir() / "replays";
                saveTarget = "Eclipse";
            }

            std::error_code ec;
            std::filesystem::create_directories(saveDir, ec);
            if (ec) {
                log::warn("Pathfinder could not create save dir {}: {}", saveDir.string(), ec.message());
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

            // The save dialog may create the entry up front, so make sure the
            // parent exists and verify the bytes actually landed afterwards —
            // only then report success.
            if (targetPath.has_parent_path()) {
                std::filesystem::create_directories(targetPath.parent_path(), ec);
            }

            auto writeRes = writeBinary(targetPath, m_macroData);
            std::error_code sizeEc;
            auto writtenSize = std::filesystem::file_size(targetPath, sizeEc);
            bool verified = writeRes.isOk() && !sizeEc && writtenSize == m_macroData.size();
            if (verified) {
                log::info("Successfully exported macro ({} bytes) to {}", m_macroData.size(), targetPath.string());
                Notification::create(
                    fmt::format("{} to {} ({} bytes)", m_solved ? "Macro exported" : "Partial macro exported", saveTarget, m_macroData.size()),
                    m_solved ? NotificationIcon::Success : NotificationIcon::Warning)->show();
            } else {
                std::string reason;
                if (writeRes.isErr()) reason = writeRes.unwrapErr();
                else if (sizeEc) reason = fmt::format("could not verify file size: {}", sizeEc.message());
                else reason = fmt::format("size mismatch: disk {} bytes, expected {}", writtenSize, m_macroData.size());
                log::error("Failed to write macro to {}: {}", targetPath.string(), reason);
                // Best-effort cleanup of the 0-byte orphan the picker may
                // have created; never touch a file that has content.
                if (!sizeEc && writtenSize == 0) {
                    std::filesystem::remove(targetPath, ec);
                }
                Notification::create(fmt::format("Failed to write macro file: {}", reason), NotificationIcon::Error)->show();
            }

            queueInMainThread([this] {
                removeFromParentAndCleanup(true);
            });
        };

        if (auto menu = getChildByID("menu")) {
            if (!m_macroData.empty()) {
                Build<ButtonSprite>::create("Export", "bigFont.fnt", "GJ_button_01.png")
                    .intoMenuItem(async::wrapSpawn(callback))
                    .scale(0.8)
                    .move(0, -40)
                    .parent(menu);
            } else {
                log::warn("Pathfinder export hidden: macro data is empty");
                if (result.error.empty()) {
                    Notification::create("Nothing to export: no inputs were recorded", NotificationIcon::Warning)->show();
                }
            }
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
        if (!CCLayerColor::initWithColor({0, 0, 0, 100})) return false;
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

        // D8: progress fill bar, kept below the percent label.
        // NOTE: pctLabel is a CCLabelBMFont, which IS-A CCSpriteBatchNode and
        // only accepts CCSprite children — parenting the CCLayerColor bars to
        // it corrupts the batch node and crashes in addChild. Parent the bars
        // to this layer instead (same coordinates: the popup menu sits at the
        // layer origin, so menu-local == layer-local).
        if (auto pctLabel = getChildByIDRecursive("percent")) {
            auto anchor = pctLabel->getPosition();
            Build<CCLayerColor>::create(ccColor4B{0, 0, 0, 150})
                .contentSize({150, 6})
                .id("bar-bg")
                .pos(anchor.x, anchor.y - 18)
                .parent(this);
            Build<CCLayerColor>::create(ccColor4B{70, 200, 70, 255})
                .contentSize({0, 6})
                .id("bar-fg")
                .pos(anchor.x, anchor.y - 18)
                .parent(this);
        } else {
            log::warn("Pathfinder popup label missing; skipping progress bar");
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
                auto node = Build<PathfinderNode>::create(m_level->m_levelName, lvlString);
                if (!node.collect()) {
                    log::error("Pathfinder failed to create solver popup");
                    Notification::create("Failed to open Pathfinder", NotificationIcon::Error)->show();
                    return;
                }
                node.parent(this).zOrder(100);
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
                auto node = Build<PathfinderNode>::create(m_level->m_levelName, lvlString);
                if (!node.collect()) {
                    log::error("Pathfinder failed to create solver popup");
                    Notification::create("Failed to open Pathfinder", NotificationIcon::Error)->show();
                    return;
                }
                node.parent(this).zOrder(100);
        }).id("pathfinder-button")
          .parent(getChildByID("other-menu"))
          .matchPos(getChildByIDRecursive("list-button"))
          .move(0, 45);

        return true;
    }
};
