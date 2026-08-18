#include <Geode/Geode.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/utils/coro.hpp>
#include <Geode/modify/EditLevelLayer.hpp>
#include <Geode/modify/LevelInfoLayer.hpp>
#include <UIBuilder.hpp>
#include "pathfinder.hpp"
#include <future>

using namespace geode::prelude;
using namespace geode::utils::file;

class PathfinderNode : public CCLayerColor {
    std::atomic_bool m_stop = false;
    std::atomic<double> m_progress = 0;
    std::future<PathfindResult> m_result;
    std::string m_levelName;
    bool m_done = false;
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

    void setStatus(std::string const& status) {
        auto label = static_cast<CCLabelBMFont*>(getChildByIDRecursive("percent"));
        if (!label)
            return;

        label->setString(status.c_str());
        label->setScale(0.45f);
    }

    void finalize(PathfindResult result) {
        m_done = true;
        getChildByIDRecursive("stop")->setVisible(false);

        if (!result.error.empty()) {
            setStatus(result.error);
            return;
        }

        if (result.macro.empty()) {
            setStatus("Generated macro was empty.");
            return;
        }
 
        auto callback = [this, macro = std::move(result.macro)](this auto self) -> arc::Future<void> {
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

            auto path = co_await pick(PickMode::SaveFile, opts);
            if (path.isErr()) {
                log::error("Failed to get file from export picker");
                queueInMainThread([this] {
                    setStatus("Failed to get file.");
                });
                co_return;
            }

            auto selectedPath = path.unwrap();
            if (!selectedPath.has_value()) {
                log::info("Macro export cancelled");
                co_return;
            }

            auto writeResult = writeBinary(*selectedPath, macro);
            if (writeResult.isErr()) {
                log::error("Failed to write macro file to {}", selectedPath->string());
                queueInMainThread([this] {
                    setStatus("Failed to write file.");
                });
                co_return;
            }

            queueInMainThread([this] {
                removeFromParentAndCleanup(true);
            });
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

    bool init(std::string const& levelName, std::string const& lvlString) {
        CCLayerColor::initWithColor({0, 0, 0, 100});
        setCascadeOpacityEnabled(true);

        m_levelName = levelName;

        m_result = std::async(std::launch::async, [lvlString, this]() {
            try {
            return pathfind(lvlString, m_stop, [this](double progress) {
                if (m_progress < progress)
                    m_progress = progress;
            });
            } catch (std::exception& e) {
                log::error("{}", e.what());
                return PathfindResult{{}, "Pathfinding crashed."};
            } catch (...) {
                log::error("Unknown error while pathfinding");
                return PathfindResult{{}, "Pathfinding crashed."};
            }
        });

        setKeypadEnabled(true);

        Build(this).initTouch().schedule([this](float) {
                if (m_result.valid() && m_result.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                    finalize(m_result.get());
                    return;
                }

                if (!m_done) {
                    Build(this).intoChildRecurseID<CCLabelBMFont>("percent")
                        .string(fmt::format("{:.2f}%", m_progress).c_str());
                }
        });

        auto handle = [this](CCMenuItemSpriteExtra* it) {
            m_stop = true;

            if (it->getID() == "stop") {
                // Do not block the game thread on future::get(); the scheduled callback
                // finalizes the partial result as soon as the worker acknowledges stop.
                it->setEnabled(false);
            } else {
                removeFromParentAndCleanup(true);
            }
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
        if (!EditLevelLayer::init(p0))
            return false;

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
        if (!LevelInfoLayer::init(level, challenge))
            return false;

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
