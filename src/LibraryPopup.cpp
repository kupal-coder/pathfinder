#include "LibraryPopup.hpp"
#include "replay.hpp"

#include <Geode/ui/Notification.hpp>
#include <Geode/utils/async.hpp>
#include <Geode/utils/coro.hpp>
#include <Geode/utils/file.hpp>
#include <UIBuilder.hpp>

#include <ctime>

using namespace geode::prelude;
using namespace geode::utils::file;

namespace {

constexpr float kPopupWidth = 400.f;
constexpr float kPopupHeight = 280.f;
constexpr float kListWidth = 356.f;
constexpr float kListHeight = 186.f;
constexpr float kRowHeight = 44.f;

/// "3m ago", "2h ago", "5d ago" -- short enough for a list row.
std::string relativeTime(int64_t savedAt) {
	if (savedAt <= 0)
		return "";

	auto now = static_cast<int64_t>(std::time(nullptr));
	auto delta = now - savedAt;
	if (delta < 0)
		delta = 0;

	if (delta < 60)
		return "just now";
	if (delta < 3600)
		return fmt::format("{}m ago", delta / 60);
	if (delta < 86400)
		return fmt::format("{}h ago", delta / 3600);
	return fmt::format("{}d ago", delta / 86400);
}

std::string formatDuration(float seconds) {
	if (seconds <= 0.f)
		return "";
	int total = static_cast<int>(seconds);
	return fmt::format("{}:{:02d}", total / 60, total % 60);
}

} // namespace

// ---------------------------------------------------------------------------
// RenamePopup
// ---------------------------------------------------------------------------

RenamePopup* RenamePopup::create(std::string const& current, std::function<void(std::string const&)> onConfirm) {
	auto ret = new RenamePopup();
	if (ret->init(current, std::move(onConfirm))) {
		ret->autorelease();
		return ret;
	}
	delete ret;
	return nullptr;
}

bool RenamePopup::init(std::string const& current, std::function<void(std::string const&)> onConfirm) {
	if (!Popup::init(320.f, 140.f, "GJ_square02.png"))
		return false;

	m_onConfirm = std::move(onConfirm);

	setTitle("Rename Macro", "bigFont.fnt", .7f, 20.f);

	m_input = TextInput::create(250.f, "Macro name", "bigFont.fnt");
	m_input->setMaxCharCount(60);
	m_input->setString(current);
	m_mainLayer->addChildAtPosition(m_input, Anchor::Center, ccp(0, 6));

	Build<ButtonSprite>::create("Save", "bigFont.fnt", "GJ_button_01.png", .8f)
		.scale(.75f)
		.intoMenuItem([this](CCMenuItemSpriteExtra*) { confirm(); })
		.with([&](auto* btn) {
			m_buttonMenu->addChildAtPosition(btn, Anchor::Bottom, ccp(0, 28));
		});

	return true;
}

void RenamePopup::confirm() {
	std::string value = m_input->getString();
	auto callback = m_onConfirm;

	onClose(nullptr);

	if (!value.empty() && callback)
		callback(value);
}

// ---------------------------------------------------------------------------
// LibraryPopup
// ---------------------------------------------------------------------------

LibraryPopup* LibraryPopup::create() {
	auto ret = new LibraryPopup();
	if (ret->init()) {
		ret->autorelease();
		return ret;
	}
	delete ret;
	return nullptr;
}

bool LibraryPopup::init() {
	if (!Popup::init(kPopupWidth, kPopupHeight, "GJ_square02.png"))
		return false;

	setTitle("Macro Library", "bigFont.fnt", .8f, 22.f);

	// Scrolling list, framed with the standard comment borders so it reads as
	// a list rather than a floating block of text.
	m_list = ScrollLayer::create({kListWidth, kListHeight});
	m_list->setAnchorPoint({0.f, 0.f});
	m_list->m_contentLayer->setLayout(
		ColumnLayout::create()
			->setAxisReverse(true)
			->setAxisAlignment(AxisAlignment::End)
			->setAutoGrowAxis(kListHeight)
			->setGap(0.f)
	);
	m_mainLayer->addChildAtPosition(
		m_list, Anchor::Center, ccp(-kListWidth / 2, -kListHeight / 2 + 8));

	auto borders = ListBorders::create();
	borders->setContentSize({kListWidth + 4, kListHeight});
	m_mainLayer->addChildAtPosition(borders, Anchor::Center, ccp(0, 8));

	// Shown when the library is empty; explains how macros get here.
	m_emptyLabel = CCLabelBMFont::create(
		"No macros yet.\nSolve a level and it saves here automatically.", "chatFont.fnt");
	m_emptyLabel->setAlignment(kCCTextAlignmentCenter);
	m_emptyLabel->setScale(.6f);
	m_emptyLabel->setOpacity(140);
	m_mainLayer->addChildAtPosition(m_emptyLabel, Anchor::Center, ccp(0, 8));

	m_countLabel = CCLabelBMFont::create("", "chatFont.fnt");
	m_countLabel->setScale(.45f);
	m_countLabel->setOpacity(150);
	m_mainLayer->addChildAtPosition(m_countLabel, Anchor::Bottom, ccp(-96, 22));

	// For anyone who would rather use the file system directly.
	Build<ButtonSprite>::create("Open Folder", "bigFont.fnt", "GJ_button_05.png", .8f)
		.scale(.5f)
		.intoMenuItem([](CCMenuItemSpriteExtra*) {
			file::openFolder(macrolib::directory());
		})
		.with([&](auto* btn) {
			m_buttonMenu->addChildAtPosition(btn, Anchor::BottomRight, ccp(-58, 22));
		});

	refresh();
	return true;
}

void LibraryPopup::refresh() {
	auto entries = macrolib::list();

	m_list->m_contentLayer->removeAllChildren();

	for (size_t i = 0; i < entries.size(); ++i)
		m_list->m_contentLayer->addChild(createRow(entries[i], kListWidth, i % 2 == 1));

	m_list->m_contentLayer->updateLayout();
	m_list->scrollToTop();

	bool empty = entries.empty();
	m_emptyLabel->setVisible(empty);
	m_list->setVisible(!empty);

	m_countLabel->setString(empty
		? ""
		: fmt::format("{} macro{}", entries.size(), entries.size() == 1 ? "" : "s").c_str());
}

CCNode* LibraryPopup::createRow(macrolib::MacroEntry const& entry, float width, bool alt) {
	auto row = CCNode::create();
	row->setContentSize({width, kRowHeight});
	row->setAnchorPoint({0.f, 0.f});

	// Alternating stripe, the same idiom GD uses for its own lists.
	auto bg = CCLayerColor::create({0, 0, 0, static_cast<GLubyte>(alt ? 45 : 20)});
	bg->setContentSize({width, kRowHeight});
	bg->ignoreAnchorPointForPosition(false);
	bg->setAnchorPoint({0.f, 0.f});
	row->addChild(bg);

	// Level name, scaled down rather than allowed to overrun the buttons.
	auto name = CCLabelBMFont::create(entry.levelName.c_str(), "bigFont.fnt");
	name->setAnchorPoint({0.f, .5f});
	name->setScale(.45f);
	if (name->getScaledContentSize().width > 170.f)
		name->setScale(.45f * 170.f / name->getScaledContentSize().width);
	row->addChildAtPosition(name, Anchor::Left, ccp(10, 10), false);

	// Completion badge: green when the route finishes, orange when partial.
	// This is what you actually scan the list for.
	auto pct = CCLabelBMFont::create(
		fmt::format("{:.1f}%", entry.percent).c_str(), "bigFont.fnt");
	pct->setAnchorPoint({0.f, .5f});
	pct->setScale(.36f);
	pct->setColor(entry.solved ? ccColor3B{90, 220, 110} : ccColor3B{255, 185, 75});
	row->addChildAtPosition(pct, Anchor::Left, ccp(10, -10), false);

	std::string detail;
	if (auto d = formatDuration(entry.duration); !d.empty())
		detail += d;
	if (entry.inputCount) {
		if (!detail.empty()) detail += "  -  ";
		detail += fmt::format("{} inputs", entry.inputCount);
	}
	if (auto when = relativeTime(entry.savedAt); !when.empty()) {
		if (!detail.empty()) detail += "  -  ";
		detail += when;
	}

	auto info = CCLabelBMFont::create(detail.c_str(), "chatFont.fnt");
	info->setAnchorPoint({0.f, .5f});
	info->setScale(.4f);
	info->setOpacity(150);
	row->addChildAtPosition(info, Anchor::Left, ccp(60, -10), false);

	auto menu = CCMenu::create();
	menu->setContentSize({128.f, kRowHeight});
	menu->setAnchorPoint({1.f, .5f});
	menu->setPosition({width - 6.f, kRowHeight / 2});
	row->addChild(menu);

	// Export is the primary action, so it gets the green button.
	Build<ButtonSprite>::create("Export", "bigFont.fnt", "GJ_button_01.png", .8f)
		.scale(.44f)
		.intoMenuItem([this, entry](CCMenuItemSpriteExtra*) { onExport(entry); })
		.pos(-100.f, 0.f)
		.parent(menu);

	Build<ButtonSprite>::create("Rename", "bigFont.fnt", "GJ_button_05.png", .8f)
		.scale(.44f)
		.intoMenuItem([this, entry](CCMenuItemSpriteExtra*) { onRename(entry); })
		.pos(-54.f, 0.f)
		.parent(menu);

	Build<CCSprite>::createSpriteName("GJ_deleteIcon_001.png")
		.scale(.62f)
		.intoMenuItem([this, entry](CCMenuItemSpriteExtra*) { onDelete(entry); })
		.pos(-14.f, 0.f)
		.parent(menu);

	return row;
}

void LibraryPopup::onExport(macrolib::MacroEntry const& entry) {
	auto botDir = macrolib::detectedBotFolder();
	auto botName = macrolib::detectedBotName();

	auto pickManually = [entry]() -> arc::Future<void> {
		FilePickOptions opts(
			entry.path.filename(), {{
			std::string("Macro File"),
			std::unordered_set{std::string("gdr2")}
		}});

		auto path = co_await pick(PickMode::SaveFile, opts);
		if (path.isOk() && path.unwrap().has_value()) {
			auto res = macrolib::exportTo(entry.path, *path.unwrap());
			queueInMainThread([res]() {
				if (res)
					Notification::create("Macro exported", NotificationIcon::Success)->show();
				else
					Notification::create("Export failed", NotificationIcon::Error)->show();
			});
		}
	};

	// If a known bot is installed, offer the one-tap path to its folder and
	// keep the file picker as the alternative.
	if (botDir && botName) {
		createQuickPopup(
			"Export Macro",
			fmt::format("Send <cy>{}</c> to <cg>{}</c>, or choose a folder yourself?",
				entry.levelName, *botName),
			"Choose...", botName->c_str(),
			[entry, botDir, botName, pickManually](FLAlertLayer*, bool toBot) {
				if (!toBot) {
					async::spawn(pickManually);
					return;
				}

				auto target = *botDir / entry.path.filename();
				auto res = macrolib::exportTo(entry.path, target);
				if (res) {
					Notification::create(
						fmt::format("Sent to {}", *botName), NotificationIcon::Success)->show();
				} else {
					Notification::create(
						"Export failed: " + res.unwrapErr(), NotificationIcon::Error)->show();
				}
			}
		);
		return;
	}

	async::spawn(pickManually);
}

void LibraryPopup::onRename(macrolib::MacroEntry const& entry) {
	auto path = entry.path;

	// The confirmation outlives this call, and the library popup can be closed
	// while it is open. Hold a Ref so the callback cannot touch a freed popup,
	// and check the node is still in the scene before refreshing it.
	Ref<LibraryPopup> self = this;

	RenamePopup::create(entry.stem(), [self, path](std::string const& name) {
		auto res = macrolib::rename(path, name);
		if (!res) {
			Notification::create("Rename failed: " + res.unwrapErr(), NotificationIcon::Error)->show();
			return;
		}
		if (self->getParent())
			self->refresh();
	})->show();
}

void LibraryPopup::onDelete(macrolib::MacroEntry const& entry) {
	auto path = entry.path;
	Ref<LibraryPopup> self = this;

	createQuickPopup(
		"Delete Macro",
		fmt::format("Delete <cy>{}</c>? This cannot be undone.", entry.levelName),
		"Cancel", "Delete",
		[self, path](FLAlertLayer*, bool confirmed) {
			if (!confirmed)
				return;

			auto res = macrolib::remove(path);
			if (!res) {
				Notification::create("Delete failed: " + res.unwrapErr(), NotificationIcon::Error)->show();
				return;
			}
			if (self->getParent())
				self->refresh();
		}
	);
}
