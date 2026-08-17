#pragma once
#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/ScrollLayer.hpp>
#include <Geode/ui/TextInput.hpp>
#include <functional>
#include "library.hpp"

/**
 * Small prompt for entering a macro name.
 *
 * A dedicated popup rather than an FLAlertLayer with an input bolted on: the
 * alert's buttons are only reachable by index, which breaks as soon as the
 * layout changes.
 */
class RenamePopup : public geode::Popup {
public:
	static RenamePopup* create(std::string const& current, std::function<void(std::string const&)> onConfirm);

protected:
	bool init(std::string const& current, std::function<void(std::string const&)> onConfirm);

private:
	geode::TextInput* m_input = nullptr;
	std::function<void(std::string const&)> m_onConfirm;

	void confirm();
};

/**
 * The macro library browser.
 *
 * Lists every macro the solver has saved, newest first, and lets you rename,
 * export or delete each one. Styled to match the rest of the mod: GJ_square02
 * background, bigFont headings, chatFont detail text.
 */
class LibraryPopup : public geode::Popup {
public:
	static LibraryPopup* create();

	/// Rebuild the list from disk. Called on open and after any change.
	void refresh();

protected:
	bool init();

private:
	geode::ScrollLayer* m_list = nullptr;
	cocos2d::CCLabelBMFont* m_emptyLabel = nullptr;
	cocos2d::CCLabelBMFont* m_countLabel = nullptr;

	cocos2d::CCNode* createRow(macrolib::MacroEntry const& entry, float width, bool alt);

	void onExport(macrolib::MacroEntry const& entry);
	void onRename(macrolib::MacroEntry const& entry);
	void onDelete(macrolib::MacroEntry const& entry);
};
