#include <Geode/Geode.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/utils/coro.hpp>
#include <Geode/modify/EditLevelLayer.hpp>
#include <Geode/modify/LevelInfoLayer.hpp>
#include <UIBuilder.hpp>
#include <Geode/ui/Notification.hpp>
#include "pathfinder.hpp"
#include "library.hpp"
#include "LibraryPopup.hpp"
#include <future>
#include <mutex>

using namespace geode::prelude;
using namespace geode::utils::file;

class PathfinderNode : public CCLayerColor {
    std::atomic_bool m_stop = false;
    std::future<PathfindResult> m_result;
    std::string m_levelName;
    uint32_t m_levelId = 0;

    /// Progress is published from the search thread and read on the main
    /// thread each frame, so it is guarded rather than passed through a pile
    /// of separate atomics.
    std::mutex m_progressMutex;
    PathfindProgress m_progress;
    bool m_finalized = false;

public:
    static PathfinderNode* create(std::string const& levelName, uint32_t levelId, std::string const& lvlString) {
        auto node = new PathfinderNode();
        if (node && node->init(levelName, levelId, lvlString)) {
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

    PathfindProgress snapshot() {
        std::lock_guard<std::mutex> lock(m_progressMutex);
        return m_progress;
    }

    void setStatus(std::string const& text) {
        if (auto label = typeinfo_cast<CCLabelBMFont*>(getChildByIDRecursive("status")))
            label->setString(text.c_str());
    }

    void setDetail(std::string const& text) {
        if (auto label = typeinfo_cast<CCLabelBMFont*>(getChildByIDRecursive("detail")))
            label->setString(text.c_str());
    }

    void finalize(PathfindResult result) {
        if (m_finalized)
            return;
        m_finalized = true;

        if (auto stopBtn = getChildByIDRecursive("stop"))
            stopBtn->setVisible(false);

        // A run that found nothing used to export an empty macro and look like
        // a success. Say what happened instead.
        if (result.macro.empty()) {
            setStatus(result.solved ? "Nothing to export" : "No macro found");
            setDetail(result.error.empty()
                ? fmt::format("Reached {:.2f}%", result.percent)
                : result.error);
            return;
        }

        // Save straight into the mod's own library. Previously the only way to
        // keep a result was to immediately drive a file picker into some other
        // mod's folder; now the macro is safe as soon as it exists, and
        // exporting is a separate, optional step.
        auto saved = macrolib::save(result.macro, m_levelName);

        if (!saved) {
            setStatus("Could not save");
            setDetail(saved.unwrapErr());
            return;
        }

        // A route computed against a world we could not fully model is not a
        // verified solve. Saying "Solved!" there would be the worst outcome:
        // the user has no way to tell the macro will kill them until it does.
        if (result.approximate) {
            setStatus(result.solved ? "Route found (unverified)" : "Partial route");
            setDetail(result.approximation.empty()
                ? "Level uses features the simulator cannot model"
                : result.approximation);
        } else if (result.solved) {
            setStatus("Solved!");
            setDetail("Saved to your macro library");
        } else {
            setStatus("Partial route");
            setDetail(fmt::format("Reached {:.2f}% - saved to library", result.percent));
        }

        auto menu = getChildByID("menu");

        Build<ButtonSprite>::create("Open Library", "bigFont.fnt", "GJ_button_01.png", .8f)
            .scale(.7f)
            .intoMenuItem([this](CCMenuItemSpriteExtra*) {
                LibraryPopup::create()->show();
                removeFromParentAndCleanup(true);
            })
            .move(-52, -40)
            .parent(menu);

        Build<ButtonSprite>::create("Done", "bigFont.fnt", "GJ_button_04.png", .8f)
            .scale(.7f)
            .intoMenuItem([this](CCMenuItemSpriteExtra*) {
                removeFromParentAndCleanup(true);
            })
            .move(52, -40)
            .parent(menu);
    }

    void keyBackClicked() override  {
        m_stop = true;
        CCLayer::keyBackClicked();
        removeFromParentAndCleanup(true);
    }

    bool init(std::string const& levelName, uint32_t levelId, std::string const& lvlString) {
        CCLayerColor::initWithColor({0, 0, 0, 100});
        setCascadeOpacityEnabled(true);

        m_levelName = levelName;
        m_levelId = levelId;

        m_result = std::async(std::launch::async, [lvlString, levelName, levelId, this]() {
            PathfindOptions options;
            // Leave a core free so the game keeps rendering while searching.
            unsigned hw = std::thread::hardware_concurrency();
            options.threads = hw > 2 ? hw - 1 : 1;
            options.level.name = levelName;
            options.level.id = levelId;

            return pathfind(lvlString, m_stop, [this](PathfindProgress const& p) {
                std::lock_guard<std::mutex> lock(m_progressMutex);
                m_progress = p;
            }, options);
        });

        setKeypadEnabled(true);

        Build(this).initTouch().schedule([this](float) {
                if (!m_finalized) {
                    auto p = snapshot();

                    Build(this).intoChildRecurseID<CCLabelBMFont>("percent")
                        .string(fmt::format("{:.2f}%", p.percent).c_str());

                    // The old UI only ever showed a high-water mark, so a
                    // search that was thrashing looked identical to one making
                    // progress. Surface the stall explicitly.
                    if (p.stalledRounds > 30) {
                        setStatus("Backtracking...");
                        setDetail(fmt::format("exploring alternatives - {} states", p.frontier));
                    } else {
                        setStatus("Path finding...");
                        setDetail(fmt::format("{} states - {}k frames",
                            p.frontier, p.framesSimulated / 1000));
                    }
                }

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
                .contentSize(280, 160),
            Build<CCLabelBMFont>::create("Path finding...", "bigFont.fnt")
                .id("status")
                .move(0, 58)
                .scale(0.7),
            Build<CCLabelBMFont>::create("0.00%", "bigFont.fnt")
                .id("percent")
                .move(0, 22)
                .scale(0.9),
            Build<CCLabelBMFont>::create("starting", "chatFont.fnt")
                .id("detail")
                .move(0, -6)
                .scale(0.65),
            Build<ButtonSprite>::create("Stop", "bigFont.fnt", "GJ_button_04.png")
                .scale(0.8)
                .intoMenuItem(handle)
                .id("stop")
                .move(0, -40),
            Build<CCSprite>::createSpriteName("GJ_closeBtn_001.png")
                .intoMenuItem(handle)
                .id("close")
                .move(-140, 80)
                .scale(0.8)
        );

        return true;
    }

};

/// Shared by both entry points so the button behaves identically in each.
static void startPathfinder(CCNode* parent, GJGameLevel* level) {
    auto lvlString = ZipUtils::decompressString(level->m_levelString, true, 0);

    if (lvlString.empty()) {
        FLAlertLayer::create(
            "Path Finding Pro",
            "This level has no data to simulate. Try opening it in the editor first.",
            "OK")->show();
        return;
    }

    Build<PathfinderNode>::create(
        level->m_levelName,
        static_cast<uint32_t>(level->m_levelID.value()),
        lvlString
    ).parent(parent).zOrder(100);
}

/// The library button, added next to the solver button on both level screens.
static CCMenuItemSpriteExtra* createLibraryButton() {
    auto spr = Build<BasedButtonSprite>::create(
        CCSprite::createWithSpriteFrameName("GJ_playBtn2_001.png"),
        BaseType::Circle,
        4,
        3
    ).scale(0.8);

    spr->setTopRelativeScale(0.7f);

    return Build<BasedButtonSprite>(spr)
        .intoMenuItem([]() {
            LibraryPopup::create()->show();
        })
        .id("pathfinder-library-button")
        .collect();
}

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
                startPathfinder(this, m_level);
        }).id("pathfinder-button")
          .intoNewParent(CCMenu::create())
          .parent(this)
          .id("pathfinder-menu")
          .matchPos(getChildByIDRecursive("delete-button"))
          .move(-45, 0)
          .with([](CCMenu* menu) {
              // Sits just below the solver button so both are reachable
              // without hunting through another mod's menus.
              auto lib = createLibraryButton();
              lib->setPosition({0, -45});
              menu->addChild(lib);
          });

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
                startPathfinder(this, m_level);
        }).id("pathfinder-button")
          .parent(getChildByID("other-menu"))
          .matchPos(getChildByIDRecursive("list-button"))
          .move(0, 45);

        if (auto otherMenu = getChildByID("other-menu")) {
            auto lib = createLibraryButton();
            Build(lib)
                .parent(otherMenu)
                .matchPos(getChildByIDRecursive("list-button"))
                .move(0, 90);
        }

        return true;
    }
};
