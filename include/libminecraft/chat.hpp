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
#include <vector>
#include "libminecraft/union.hpp"
#include "libminecraft/iobase.hpp"

/// @brief The chat component whose trait is text.
struct McDtChatTraitText {
	std::u16string text;		///< The chat to be shown directly.
};

/// @brief The chat component whose trait is translate.
struct McDtChatTraitTranslate {
	std::string translate;				///< The translation key from user locale.
	std::vector<std::u16string> with;	///< To content to be placed in.
};

/// @brief The chat component which forwards a keybind.
struct McDtChatTraitKeybind {
	const char* key;			///< The name of the key in option.txt, 
								///  generated from gperf's key list.
};

/// @brief The chat component which forwards an scoreboard item.
struct McDtChatTraitScore {
	mc::ustring<16> objective;	///< The objective string (Notice the constrain).
	std::u16string name;		///< The player name, which might be u16string.
	std::u16string value;		///< The score value of the object.
};

/// @brief Defines the union info for chat traits.
typedef mc::cuinfo<McDtChatTraitText, McDtChatTraitTranslate,
			McDtChatTraitKeybind, McDtChatTraitScore> McDtChatTraitInfo;

/// @brief The click event to open a url.
struct McDtChatClickOpenUrl {
	std::u16string url;			///< The url, either in http:// or https://
};

/// @brief The click event to run a command.
struct McDtChatClickRunCommand {
	std::u16string command;		///< The command to run.
};

/// @brief The click event to suggest a command (replace chat input line).
struct McDtChatClickExecuteCommand {
	std::u16string command;		///< The command to suggest.
};

/// @brief The click event to suggest change page in book.
struct McDtChatClickChangePage {
	size_t pageNo;				///< The number of page to change to.
};

/// @brief Defines the union info for chat click.
typedef mc::cuinfo<McDtChatClickOpenUrl, McDtChatClickRunCommand,
	McDtChatClickExecuteCommand, McDtChatClickChangePage> McDtChatClickInfo;

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

/// Making up the final chat compound class.
struct McDtChatCompound {
	bool bold : 1;				///< Whether modified by bold decoration.
	bool italic : 1;			///< Whether modified by italic decoration.
	bool underlined : 1;		///< Whether modified by underline decoration.
	bool strikethrough : 1;		///< Whether modified by strike through decoration.
	bool obfuscated : 1;		///< Whether modified by obfuscated decoration.
	
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
	std::vector<McDtChatCompound> extra;
	
	/// Inherit style modifier from another compound.
	inline void inheritStyle(const McDtChatCompound& parent) {
		bold = parent.bold;
		italic = parent.italic;
		underlined = parent.underlined;
		strikethrough = parent.strikethrough;
		obfuscated = parent.obfuscated;
		color = parent.color;
	}
};

// The I/O methods for reading and writing an McDtChatCompound. Please notice 
// the streams are not number prefixed.
void McIoReadChatCompound(McIoInputStream& inputStream, McDtChatCompound& compound, 
			size_t expectedSize, bool tolerant = false);
void McIoWriteChatCompound(McIoOutputStream& outputStream,
			const McDtChatCompound& compound);

// Forward namespace definitions for mc::chat and mc::chatcolor.
class McDtFlavourChatCompound;
namespace mc {
namespace chatcolor {	// Enumerate possible chat colors.
constexpr const McDtChatColor* reset        = &McDtChatColor::reset;
constexpr const McDtChatColor* black        = &McDtChatColor::colors[0];
constexpr const McDtChatColor* dark_blue    = &McDtChatColor::colors[1];
constexpr const McDtChatColor* dark_green   = &McDtChatColor::colors[2];
constexpr const McDtChatColor* dark_aqua    = &McDtChatColor::colors[3];
constexpr const McDtChatColor* dark_red     = &McDtChatColor::colors[4];
constexpr const McDtChatColor* dark_purple  = &McDtChatColor::colors[5];
constexpr const McDtChatColor* gold         = &McDtChatColor::colors[6];
constexpr const McDtChatColor* gray         = &McDtChatColor::colors[7];
constexpr const McDtChatColor* dark_gray    = &McDtChatColor::colors[8];
constexpr const McDtChatColor* blue         = &McDtChatColor::colors[9];
constexpr const McDtChatColor* green        = &McDtChatColor::colors[10];
constexpr const McDtChatColor* aqua         = &McDtChatColor::colors[11];
constexpr const McDtChatColor* red          = &McDtChatColor::colors[12];
constexpr const McDtChatColor* light_purple = &McDtChatColor::colors[13];
constexpr const McDtChatColor* yellow       = &McDtChatColor::colors[14];
constexpr const McDtChatColor* white        = &McDtChatColor::colors[15];
};
typedef McDtDataType<McDtChatCompound, McDtFlavourChatCompound> chat;
} // End of namespace mc.