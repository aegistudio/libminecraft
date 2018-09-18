#pragma once
/**
 * @file libminecraft/chat.hpp
 * @brief The minecraft chat structure
 * @author Haoran Luo
 *
 * Defines data types related to minecraft chat. Defining minecraft chat in 
 * EBNF (Extended Backus-Naur Form) as follow:
 *
 * '''
 * // Basic Modifers (Applicable to all chat types).
 * decoration ::= bold | italic | underlined | strikethrough | obfuscated
 * color ::= black | darkBlue | darkGreen | darkAqua | darkRed | 
 *			darkPurple | gold | gray | darkGray | blue | green |
 *			aqua | red | lightPurple | yellow | white
 * clickEvent ::= openUrl url | runCommand command | 
 *			suggestCommand command | changePage pageNo
 * hoverEvent ::= showText text | showItem item | showEntity entity
					| showAchievement achievement
 * modifier ::= (decoration)* [color] [clickEvent] [hoverEvent] 
 *				[insertion InsertionText]
 *
 * // Chat Component Traits.
 * chatTrait ::= text chatText | translate key (value)* | 
 *				keybind optionName | score name objective value
 *
 * // The final chat structure.
 * chatComponent ::= chatTrait modifier (chatComponent)*
 * '''
 * 
 * The extra chat component inherits its parent's decoration and color.
 * This file reflects the upper EBNF definition respectively.
 * Depending a content should be pure English or could be localized string,
 * std::string and std::u16string will be used respectively. (For string
 * with length constrains, mc::ustring<maxLength> will be used).
 */
#include <string>
#include <list>
#include "libminecraft/union.hpp"
#include "libminecraft/iobase.hpp"

/// @brief The chat component whose trait is text.
struct McDtChatTraitText {
	std::u16string text;		///< The chat to be shown directly.
};

/// @brief The chat component whose trait is translate.
/// The with field is moved to the chat body, for easier size induction in template.
struct McDtChatTraitTranslate {
	std::string translate;				///< The translation key from user locale.	
};

/// @brief The chat component which forwards a keybind.
struct McDtChatTraitKeybind {
	const char* name;			///< The name of the key in option.txt, 
								///  generated from gperf's key list.

	// Enumerate all possible keybinds here.
	enum McDtChatKeybind {
		kbAttack = 0,
		kbUse,
		kbForward,
		kbLeft,
		kbBack,
		kbRight,
		kbJump,
		kbSneak,
		kbSprint,
		kbDropItem,
		kbOpenInventory,
		kbChat,
		kbPlayerList,
		kbPickItem,
		kbCommand,
		kbScreenShot,
		kbChangeView,
		kbSmoothCamera,
		kbFullScreen,
		kbSpectatorOutlines,
		kbSwapHands,
		kbSaveToolbar,
		kbLoadToolbar,
		kbAdvancement,
		kbHotbar,
		kbHotbar1 = kbHotbar + 0,
		kbHotbar2 = kbHotbar + 1,
		kbHotbar3 = kbHotbar + 2,
		kbHotbar4 = kbHotbar + 3,
		kbHotbar5 = kbHotbar + 4,
		kbHotbar6 = kbHotbar + 5,
		kbHotbar7 = kbHotbar + 6,
		kbHotbar8 = kbHotbar + 7,
		kbHotbar9 = kbHotbar + 8,
		kbMaxValue	// Cannot be used as index.
	};
	
	/// The value of the keybind ordinal.
	const McDtChatKeybind keybind;
	
	/// The basic chat bindings.
	static const McDtChatTraitKeybind keybinds[kbMaxValue];
	
	/// The lookup function.
	static const McDtChatTraitKeybind* lookup(const char* name, size_t length);
};

/// @brief The chat component which forwards an scoreboard item.
struct McDtChatTraitScore {
	mc::ustring<16> objective;	///< The objective string (Notice the constrain).
	std::u16string name;		///< The player name, which might be u16string.
	std::u16string value;		///< The score value of the object.
};

/// @brief Defines the union info for chat traits.
typedef mc::cuinfo<McDtChatTraitText, McDtChatTraitTranslate,
			const McDtChatTraitKeybind*, McDtChatTraitScore> McDtChatTraitInfo;

/// @brief The click event to open a url.
struct McDtChatClickOpenUrl {
	std::u16string url;			///< The url, either in http:// or https://
};

/// @brief The click event to run a command.
struct McDtChatClickRunCommand {
	std::u16string command;		///< The command to run.
};

/// @brief The click event to suggest a command (replace chat input line).
struct McDtChatClickSuggestCommand {
	std::u16string command;		///< The command to suggest.
};

/// @brief The click event to suggest change page in book.
struct McDtChatClickChangePage {
	size_t pageNo;				///< The number of page to change to.
};

/// @brief Defines the union info for chat click.
typedef mc::cuinfo<McDtChatClickOpenUrl, McDtChatClickRunCommand,
	McDtChatClickSuggestCommand, McDtChatClickChangePage> McDtChatClickInfo;

/// @brief The hover event to display a text.
struct McDtChatHoverShowText {
	std::u16string text;			///< The text to show.
};

/// @brief The hover event to show an item.
struct McDtChatHoverShowItem {
	std::u16string item;			///< The item's serialized string.
};

/// @brief The hover event to show an entity.
struct McDtChatHoverShowEntity {
	// @warning might change to type, uuid, name later.
	std::u16string entity;		///< The entity's serialized string.
};

/// @brief The hover event to display an achievement.
/// Notice > 1.12 achievement is replaced with advancement and this 
/// hover event will be ignored.
struct McDtChatHoverShowAchievement {
	const char* achivement;		///< The key of the achievement.
};

/// @brief Defines the union info for chat event.
typedef mc::cuinfo<McDtChatHoverShowText, McDtChatHoverShowItem,
	McDtChatHoverShowEntity, McDtChatHoverShowAchievement> McDtChatHoverInfo;

/// @brief Defines the chat color specified.
/// Please notice that reset is included in the color.
struct McDtChatColor {
	const char* name;			///< The value of the color.
	char controlChar;			///< The control character.
	unsigned char fg[3];		///< The color when used as foreground.
	unsigned char bg[3];		///< The color when used as background.
	
	/// The enumerations for given colors.
	static const McDtChatColor colors[16];
	
	/// Reset is special color, it could be placed in chat color, and will
	/// force the compound into certain color by context.
	static const McDtChatColor reset;
	
	/// Convert a color identified by a string into one of the colors or reset.
	static const McDtChatColor* lookup(const char* colorName, size_t length);
};

/// Indicating whether the parent's decoration is override and how was it overriden.
enum McDtChatDecorationFlag {
	dcInherit = 0, 		///< When nothing is specified to the decoration.
	dcEnable = 1, 		///< When true is specified to the decoration.
	dcDisable = 2		///< When false is specified to the decoration.
};

/// Making up the final chat compound class.
struct McDtChatCompound {
	McDtChatDecorationFlag bold          : 2;	///< Whether modified by bold decoration.
	McDtChatDecorationFlag italic        : 2;	///< Whether modified by italic decoration.
	McDtChatDecorationFlag underlined    : 2;	///< Whether modified by underline decoration.
	McDtChatDecorationFlag strikethrough : 2;	///< Whether modified by strike through decoration.
	McDtChatDecorationFlag obfuscated    : 2;	///< Whether modified by obfuscated decoration.
	
	/// The color of the text. If set to null and no tag will be written out.
	const McDtChatColor* color;
	
	/// The insertion of this component.
	mc::cunion<std::u16string> insertion;	
	
	/// The content of the text. If set to null "text":"" will be written out.
	McDtUnion<McDtChatTraitInfo> content;
	
	/// The click event of the text.
	McDtUnion<McDtChatClickInfo> clickEvent;
	
	/// The hover event of the text.
	McDtUnion<McDtChatHoverInfo> hoverEvent;
	
	/// The siblings of the current chat compond. Siblings will inherit the 
	/// decoration and colors of their parents. 
	/// The size of extra is always unknown while serializing, so for efficiently
	/// implementation of I/O method, extra is designed to be a doubly-linked list.
	/// (So is the translate's with array).
	std::list<McDtChatCompound> extra;
	
	/// Only useful when chat trait is with, storing the translation data.
	std::list<McDtChatCompound> with;
};

// The I/O methods for reading and writing an McDtChatCompound. Please notice 
// the streams are not number prefixed.
/**
 * Read the chat compound data from the specified input stream.
 *
 * @param[in] inputStream the json input stream.
 * @param[out] compound the output compound data.
 * @param[in] expectedSize the size of the json portion in the stream.
 * @param[in] tolerant whether the parsing mode is tolerants.
 * @param[in] autoCompact whether performing compact in place is enabled.
 */
void McIoReadChatCompound(McIoInputStream& inputStream, McDtChatCompound& compound, 
			size_t expectedSize, bool tolerant = false, bool autoCompact = true);
void McIoWriteChatCompound(McIoOutputStream& outputStream,
			const McDtChatCompound& compound);

// Forward namespace definitions for mc::chat and mc::chatcolor.
class McDtFlavourChatCompound;
namespace mc {
namespace chatcolor {	// Enumerate possible chat colors.
constexpr const McDtChatColor* reset = &McDtChatColor::reset;
#ifndef __chatcolor_constexpr
#define __chatcolor_constexpr(name, index)\
constexpr const McDtChatColor* name = &McDtChatColor::colors[index];

__chatcolor_constexpr(black,        0)
__chatcolor_constexpr(dark_blue,    1)
__chatcolor_constexpr(dark_green,   2)
__chatcolor_constexpr(dark_aqua,    3)
__chatcolor_constexpr(dark_red,     4)
__chatcolor_constexpr(dark_purple,  5)
__chatcolor_constexpr(gold,         6)
__chatcolor_constexpr(gray,         7)
__chatcolor_constexpr(dark_gray,    8)
__chatcolor_constexpr(blue,         9)
__chatcolor_constexpr(green,        10)
__chatcolor_constexpr(aqua,         11)
__chatcolor_constexpr(red,          12)
__chatcolor_constexpr(light_purple, 13)
__chatcolor_constexpr(yellow,       14)
__chatcolor_constexpr(white,        15)

#else
#error "Macro __chatcolor_constexpr has been incorrectly defined."
#endif
};

namespace chatkeybind {	// Enumerate possible keybinds.
#ifndef __chatkeybind_constexpr
#define __chatkeybind_constexpr(name, enumerate)\
constexpr const McDtChatTraitKeybind* name = \
&McDtChatTraitKeybind::keybinds[McDtChatTraitKeybind::enumerate];

__chatkeybind_constexpr(attack,            kbAttack)
__chatkeybind_constexpr(use,               kbUse)
__chatkeybind_constexpr(forward,           kbForward)
__chatkeybind_constexpr(left,              kbLeft)
__chatkeybind_constexpr(back,              kbBack)
__chatkeybind_constexpr(right,             kbRight)
__chatkeybind_constexpr(jump,              kbJump)
__chatkeybind_constexpr(sneak,             kbSneak)
__chatkeybind_constexpr(sprint,            kbSprint)
__chatkeybind_constexpr(dropItem,          kbDropItem)
__chatkeybind_constexpr(openInventory,     kbOpenInventory)
__chatkeybind_constexpr(chat,              kbChat)
__chatkeybind_constexpr(playerList,        kbPlayerList)
__chatkeybind_constexpr(pickItem,          kbPickItem)
__chatkeybind_constexpr(command,           kbCommand)
__chatkeybind_constexpr(screenShot,        kbScreenShot)
__chatkeybind_constexpr(changeView,        kbChangeView)
__chatkeybind_constexpr(smoothCamera,      kbSmoothCamera)
__chatkeybind_constexpr(fullscreen,        kbFullScreen)
__chatkeybind_constexpr(spectatorOutlines, kbSpectatorOutlines)
__chatkeybind_constexpr(swapHands,         kbSwapHands)
__chatkeybind_constexpr(saveToolbar,       kbSaveToolbar)
__chatkeybind_constexpr(loadToolbar,       kbLoadToolbar)
__chatkeybind_constexpr(advancement,       kbAdvancement)
__chatkeybind_constexpr(hotbar1,           kbHotbar1)
__chatkeybind_constexpr(hotbar2,           kbHotbar2)
__chatkeybind_constexpr(hotbar3,           kbHotbar3)
__chatkeybind_constexpr(hotbar4,           kbHotbar4)
__chatkeybind_constexpr(hotbar5,           kbHotbar5)
__chatkeybind_constexpr(hotbar6,           kbHotbar6)
__chatkeybind_constexpr(hotbar7,           kbHotbar7)
__chatkeybind_constexpr(hotbar8,           kbHotbar8)
__chatkeybind_constexpr(hotbar9,           kbHotbar9)

#undef __chatkeybind_constexpr
#else
#error "Macro __chatkeybind_constexpr has been incorrectly defined."
#endif
};

typedef McDtDataType<McDtChatCompound, McDtFlavourChatCompound> chat;
} // End of namespace mc.