#include <Geode/Geode.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/utils/coro.hpp>
#include <Geode/modify/EditLevelLayer.hpp>
#include <Geode/modify/LevelInfoLayer.hpp>
#include <UIBuilder.hpp>
#include "checker.hpp"
#include <chrono>
#include <optional>
#include <sstream>

using namespace geode::prelude;
using namespace geode::utils::file;

class PathfinderNode : public CCLayerColor {
    std::atomic_bool m_stop = false;
    std::atomic<double> m_progress = 0;
    bool m_started = false;
    bool m_finished = false;
    std::optional<RuntimeSearchTask> m_search;
    std::string m_levelName;
    GJGameLevel* m_level = nullptr;
public:
    static PathfinderNode* create(GJGameLevel* level, std::string const& levelName, std::string const& lvlString) {
        auto node = new PathfinderNode();
        if (node && node->init(level, levelName, lvlString)) {
            node->autorelease();
            return node;
        }
        CC_SAFE_DELETE(node);
        return nullptr;
    }

    ~PathfinderNode() {
        m_stop = true;
        m_search.reset();
        CC_SAFE_RELEASE(m_level);
    }

    void finalize(std::vector<uint8_t> macro) {
        m_finished = true;
        getChildByIDRecursive("stop")->setVisible(false);

        auto verification = verifyInGame(m_level, macro);
        if (verification.completed) {
            // A second fresh attempt catches random-seed, trigger-reset, and
            // stale-object-state bugs before a replay reaches the export UI.
            auto repeat = verifyInGame(m_level, macro);
            if (!repeat.completed) {
                verification = std::move(repeat);
            } else if (repeat.traceHash != verification.traceHash) {
                const auto firstHash = verification.traceHash;
                const auto repeatHash = repeat.traceHash;
                verification = std::move(repeat);
                verification.completed = false;
                verification.error = fmt::format(
                    "runtime trace mismatch ({:016x} != {:016x})",
                    firstHash, repeatHash
                );
            }
        }
        if (!verification.completed) {
            if (verification.died) {
                log::error(
                    "Path rejected: P{} collision at frame {}; object ID {} at "
                    "({:.3f}, {:.3f}), rotation {:.3f}, scale ({:.3f}, {:.3f}); "
                    "player ({:.3f}, {:.3f}), y-velocity {:.6f}",
                    verification.player2 ? 2 : 1, verification.frame,
                    verification.objectID, verification.objectX, verification.objectY,
                    verification.objectRotation, verification.objectScaleX,
                    verification.objectScaleY, verification.playerX,
                    verification.playerY, verification.playerYVelocity
                );
            } else {
                log::error("Path rejected at frame {}: {}", verification.frame, verification.error);
            }

            const auto reason = verification.died
                ? fmt::format(
                    "The runtime-searched path died during fresh verification.\n"
                    "P{}  Frame: {}  Object: {}\n"
                    "Object: ({:.1f}, {:.1f}) rot {:.1f}\n"
                    "Player: ({:.1f}, {:.1f})\n\n"
                    "The unsafe replay was not exported.",
                    verification.player2 ? 2 : 1, verification.frame,
                    verification.objectID, verification.objectX,
                    verification.objectY, verification.objectRotation,
                    verification.playerX, verification.playerY
                )
                : fmt::format(
                    "The path could not be verified in Geometry Dash.\n{}\n\n"
                    "The unsafe replay was not exported.", verification.error
                );
            FLAlertLayer::create("Path rejected", reason, "OK")->show();
            removeFromParentAndCleanup(true);
            return;
        }

        auto callback = [this, macro](this auto self) -> arc::Future<void> {
            auto saveDir = Mod::get()->getSaveDir();
            if (Loader::get()->isModLoaded("eclipse.eclipse-menu")) {
                saveDir = Loader::get()->getLoadedMod("eclipse.eclipse-menu")->getSaveDir() / "replays";
            }

            if (!exists(saveDir)) {
                create_directories(saveDir);
            }

            FilePickOptions opts(
                saveDir / fmt::format("{}.gdr2", m_levelName), {{
                std::string("Macro File"),
                std::unordered_set {std::string("gdr2")}
            }});

            if (auto path = co_await pick(PickMode::SaveFile, opts); path.isOk() && path.unwrap().has_value()) {
                (void)writeBinary(*path.unwrap(), macro);
                queueInMainThread([this] {
                    removeFromParentAndCleanup(true);
                });
            }
        };

        Build<ButtonSprite>::create("Export", "bigFont.fnt", "GJ_button_01.png")
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

    bool init(GJGameLevel* level, std::string const& levelName, std::string const& lvlString) {
        CCLayerColor::initWithColor({0, 0, 0, 100});
        setCascadeOpacityEnabled(true);

        m_level = level;
        CC_SAFE_RETAIN(m_level);
        m_levelName = levelName;

        (void)lvlString; // Runtime PlayLayer is now the authoritative level source.
        setKeypadEnabled(true);

        Build(this).initTouch().schedule([this](float) {
            Build(this).intoChildRecurseID<CCLabelBMFont>("percent")
                .string(fmt::format("{:.2f}%", m_progress).c_str());
            if (m_finished)
                return;

            try {
                if (!m_started) {
                    m_started = true;
                    m_search.emplace(pathfindInGame(m_level, m_stop, [this](double value) {
                        if (m_progress < value)
                            m_progress = value;
                    }));
                }

                // Keep search on the cocos thread (required by PlayLayer), but
                // resume only within a small frame budget so Stop/Back and the
                // progress UI remain responsive.
                const auto deadline = std::chrono::steady_clock::now() +
                    std::chrono::milliseconds(4);
                bool done = false;
                do {
                    done = m_search->resume();
                } while (!done && std::chrono::steady_clock::now() < deadline);

                if (done) {
                    auto macro = m_search->takeResult();
                    m_search.reset();
                    finalize(std::move(macro));
                }
            } catch (std::exception const& error) {
                log::error("Runtime pathfinder failed: {}", error.what());
                m_search.reset();
                finalize({});
            }
        });

        auto handle = [this](CCMenuItemSpriteExtra* it) {
            m_stop = true;

            // A stop request rejects the incomplete candidate rather than
            // exporting an unverified replay.
            removeFromParentAndCleanup(true);
        };

        auto menu = Build<CCMenu>::create().parent(this).id("menu").children(
            Build<CCScale9Sprite>::create("GJ_square02.png")
                .contentSize(250, 140),
            Build<CCLabelBMFont>::create("Pathfinding...", "bigFont.fnt")
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

        return true;
    }

};


class AcceptanceRunnerNode : public CCLayerColor {
    std::vector<std::filesystem::path> m_fixtures;
    size_t m_index = 0;
    std::optional<RuntimeSearchTask> m_search;
    GJGameLevel* m_level = nullptr;
    std::atomic_bool m_stop = false;
    double m_levelProgress = 0;
    std::stringstream m_report;

    void releaseLevel() {
        CC_SAFE_RELEASE_NULL(m_level);
    }

    void finish() {
        m_search.reset();
        releaseLevel();
        auto reportPath = Mod::get()->getSaveDir() / "acceptance-report.txt";
        (void)writeString(reportPath, m_report.str());
        const bool stopped = m_stop;
        FLAlertLayer::create(
            stopped ? "Acceptance stopped" : "Acceptance complete",
            fmt::format("Tested {}/{} fixtures.\nReport: {}",
                m_index, m_fixtures.size(), reportPath.string()),
            "OK"
        )->show();
        removeFromParentAndCleanup(true);
    }

    bool startFixture() {
        if (m_index >= m_fixtures.size())
            return false;
        auto raw = readString(m_fixtures[m_index]);
        if (raw.isErr()) {
            m_report << m_fixtures[m_index].filename().string() << ": READ FAILED\n";
            ++m_index;
            return true;
        }

        m_level = GJGameLevel::create();
        m_level->retain();
        m_level->m_levelName = m_fixtures[m_index].stem().string();
        m_level->m_levelString = ZipUtils::compressString(raw.unwrap(), true, 0);
        m_levelProgress = 0;
        m_search.emplace(pathfindInGame(m_level, m_stop, [this](double value) {
            m_levelProgress = std::max(m_levelProgress, value);
        }));
        return true;
    }

    void step(float) {
        if (m_stop) {
            finish();
            return;
        }
        if (!m_search && !startFixture()) {
            finish();
            return;
        }
        if (!m_search)
            return;

        auto label = typeinfo_cast<CCLabelBMFont*>(getChildByID("status"));
        if (label) {
            label->setString(fmt::format(
                "{}/{}  {}  {:.1f}%", m_index + 1, m_fixtures.size(),
                m_fixtures[m_index].stem().string(), m_levelProgress
            ).c_str());
        }

        try {
            const auto deadline = std::chrono::steady_clock::now() +
                std::chrono::milliseconds(4);
            bool done = false;
            do {
                done = m_search->resume();
            } while (!done && std::chrono::steady_clock::now() < deadline);
            if (!done)
                return;

            auto macro = m_search->takeResult();
            m_search.reset();
            auto first = verifyInGame(m_level, macro);
            auto second = first.completed ? verifyInGame(m_level, macro) : first;
            const bool pass = first.completed && second.completed &&
                first.traceHash == second.traceHash;
            m_report << m_fixtures[m_index].filename().string() << ": "
                     << (pass ? "PASS" : "FAIL")
                     << " frame=" << (pass ? first.frame : second.frame)
                     << " object=" << (pass ? 0 : second.objectID)
                     << " position=(" << second.objectX << ',' << second.objectY << ')'
                     << " error=" << second.error << '\n';
            ++m_index;
            releaseLevel();
        } catch (std::exception const& error) {
            m_report << m_fixtures[m_index].filename().string()
                     << ": ERROR " << error.what() << '\n';
            m_search.reset();
            releaseLevel();
            ++m_index;
        }
    }

public:
    static AcceptanceRunnerNode* create() {
        auto node = new AcceptanceRunnerNode();
        if (node && node->init()) {
            node->autorelease();
            return node;
        }
        CC_SAFE_DELETE(node);
        return nullptr;
    }

    ~AcceptanceRunnerNode() override {
        m_stop = true;
        m_search.reset();
        releaseLevel();
    }

    bool init() override {
        if (!CCLayerColor::initWithColor({0, 0, 0, 180}))
            return false;
        for (auto const& entry : std::filesystem::recursive_directory_iterator(
                 Mod::get()->getResourcesDir())) {
            if (entry.is_regular_file() && entry.path().extension() == ".lvl")
                m_fixtures.push_back(entry.path());
        }
        std::sort(m_fixtures.begin(), m_fixtures.end());
        setKeypadEnabled(true);
        Build<CCLabelBMFont>::create("Preparing acceptance tests...", "bigFont.fnt")
            .id("status").scale(.5f).parent(this);
        Build<ButtonSprite>::create("Stop", "bigFont.fnt", "GJ_button_06.png")
            .intoMenuItem([this](auto*) { m_stop = true; })
            .move(0, -45).intoNewParent(CCMenu::create()).parent(this);
        schedule(schedule_selector(AcceptanceRunnerNode::step));
        return true;
    }

    void keyBackClicked() override { m_stop = true; }
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
                Build<PathfinderNode>::create(m_level, m_level->m_levelName, lvlString).parent(this).zOrder(100);
        }).id("pathfinder-button")
          .intoNewParent(CCMenu::create())
          .parent(this)
          .id("pathfinder-menu")
          .matchPos(getChildByIDRecursive("delete-button"))
          .move(-45, 0);

        Build<ButtonSprite>::create("Tests", "bigFont.fnt", "GJ_button_05.png")
            .scale(.45f)
            .intoMenuItem([this](auto*) {
                Build<AcceptanceRunnerNode>::create().parent(this).zOrder(200);
            })
            .move(0, -42)
            .parent(getChildByID("pathfinder-menu"));

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
                Build<PathfinderNode>::create(m_level, m_level->m_levelName, lvlString).parent(this).zOrder(100);
        }).id("pathfinder-button")
          .parent(getChildByID("other-menu"))
          .matchPos(getChildByIDRecursive("list-button"))
          .move(0, 45);

        return true;
    }
};
