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
    std::future<std::vector<uint8_t>> m_result;
    bool m_finalized = false;
    std::vector<uint8_t> m_macroData;
    std::string m_levelName;
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

    void finalize(std::vector<uint8_t> macro) {
        if (m_finalized) return;
        m_finalized = true;
        m_macroData = macro;

        if (auto stopBtn = getChildByIDRecursive("stop")) {
            stopBtn->setVisible(false);
        }
 
        auto callback = [this](this auto self) -> arc::Future<void> {
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
                    Notification::create("Macro exported successfully!", NotificationIcon::Success)->show();
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
                return std::vector<uint8_t>();
            }
        });

        setKeypadEnabled(true);

        Build(this).initTouch().schedule([this](float) {
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
