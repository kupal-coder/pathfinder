#include <Geode/Geode.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/utils/coro.hpp>
#include <Geode/modify/EditLevelLayer.hpp>
#include <Geode/modify/LevelInfoLayer.hpp>
#include <UIBuilder.hpp>
#include "checker.hpp"

using namespace geode::prelude;
using namespace geode::utils::file;

class PathfinderNode : public CCLayerColor {
    std::atomic_bool m_stop = false;
    std::atomic<double> m_progress = 0;
    bool m_started = false;
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
        CC_SAFE_RELEASE(m_level);
    }

    void finalize(std::vector<uint8_t> macro) {
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

            if (!m_started) {
                m_started = true;
                try {
                    auto macro = pathfindInGame(m_level, m_stop, [this](double value) {
                        if (m_progress < value)
                            m_progress = value;
                    });
                    finalize(std::move(macro));
                } catch (std::exception const& error) {
                    log::error("Runtime pathfinder failed: {}", error.what());
                    finalize({});
                }
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
