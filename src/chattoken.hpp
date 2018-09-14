/**
 * @file chattoken.hpp
 * @brief Headers for chat tokenizing related objects.
 * @author Haoran Luo
 *
 * This file specifies result after parsing chat key or special 
 * string fields. The result is always returned with some extra
 * metadata, as gperf provide us with such utilities.
 */
#include <cstdint>
#include <string>
#include "jsontoken.hpp"
#include "libminecraft/chat.hpp"
 
// Specifies where could a key exists.
enum McDtChatParseContext {
	cpcGenesis       = 1,
	cpcChatCompound  = 2,
	cpcHover         = 4,
	cpcClick         = 8,
	cpcExtra         = 16,
	cpcWith          = 32,
	cpcScore         = 64,
	cpcHoverAct      = 128,
	cpcClickAct      = 256,
	cpcBoolean       = 512
};

// Specify what token has it encountered.
enum McDtChatParseKey {
	// The boolean string.
	keyTrue,
	keyFalse,
	
	// Populate array.
	keyExtra,
	
	// Basic compound attributes.
	keyBold,
	keyItalic,
	keyUnderlined,
	keyStrikeThrough,
	keyObfuscated,
	keyInsertion,
	keyColor,
	
	// Type traits.
	keyText,
	keyTranslate,
	keyWith,
	keyBind,
	keyScore,
	keyPlayerName,
	keyObjective,
	
	// Hover event.
	keyHoverEvent,
	keyClickEvent,
	keyAction,
	keyValue,
	
	// The actions.
	keyActOpenUrl,
	keyActRunCmd,
	keyActSuggestCmd,
	keyActChangePage,
	keyActShowItem,
	keyActShowEntity,
};

// Specifies the types for a key.
struct McDtChatParseToken {
	// The original name of the token, used by the perfect hashing function's input.
	const char* name;
	
	// The ordinal of the token, as is defined above.
	McDtChatParseKey tokenKey;
	
	// Under which context are the tokens permitted.
	/*McDtChatParseContext*/ unsigned short acceptedContext;
	
	// What are the types of the token.
	unsigned short acceptedType;
	
	// The static reverse lookup function of a token.
	static const McDtChatParseToken* lookup(const char* name, size_t length);
};