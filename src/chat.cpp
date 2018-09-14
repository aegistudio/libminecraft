/**
 * @file chat.cpp
 * @brief Implementation for chat modules.
 * @author Haoran Luo
 *
 * For interface specification, please refer to the corresponding header.
 * @see libminecraft/chat.hpp
 */
#include "libminecraft/chat.hpp"
#include "libminecraft/bufstream.hpp"
#include "rapidjson/rapidjson.h"
#include "jsontoken.hpp"
#include "chattoken.hpp"
#include <stdexcept>
#include <stack>
#include <memory>

// Implementation for the forwarded McDtDataType methods.
template<>
McIoInputStream& mc::chat::read(McIoInputStream& inputStream) {
	// Ensure a valid length is read.
	mc::var32 length; inputStream >> length;
	if(length <= 0 || length > 32767) 
		throw std::runtime_error("Malformed chat message.");
	
	// Read real data after the length.
	McIoReadChatCompound(inputStream, data, length);
	return inputStream;
}

template<>
McIoOutputStream& mc::chat::write(McIoOutputStream& outputStream) const {
	// Serialize the chat into a buffer.
	McIoBufferOutputStream bufferStream;
	McIoWriteChatCompound(bufferStream, data);
	
	// Retrieve and validate the chat data.
	size_t chatLength; const char* chatData;
	std::tie(chatLength, chatData) = bufferStream.rawData();
	if(chatLength > 32767) throw std::runtime_error("Chat is too long.");
	
	// Write out the chat.
	mc::s32 length = chatLength; outputStream << length;
	outputStream.write(chatData, chatLength);
	return outputStream;
}

/// The event work data maintaining action and value for events composing
/// of action and value, where action determines the type of that field,
/// and value determines the data of the field.
/// The reference to corresponding event field must be provided.
template<typename EventFieldHandler>
struct McDtChatEventWorkData : public McDtJsonWorkData {
	typedef typename EventFieldHandler::eventFieldType eventFieldType;
	typedef typename EventFieldHandler::tokenType tokenType;
	
	// Containing the value when it is an utf-8 string.
	std::string valueString;
	
	// Containing the value when it is an integer.
	uint64_t valueInteger;
	
	// The reference to the event field.
	eventFieldType& eventField;
	
	// The stored action token.
	tokenType storedActionToken;
	
	// Track state for the working data, 0 for not specified,
	// 1 for value string, 2 for value integer.
	enum dataState {
		vNone = 0, vString = 1, vInteger = 2, 
		vInitialized = 3, vCompleted = 4
	};
	dataState state;
	
	// Initialize the event work data.
	McDtChatEventWorkData(eventFieldType& eventField): 
		valueString(), valueInteger(0), state(vNone), 
		eventField(eventField) {}
	
	// Destruct the event work data.
	virtual ~McDtChatEventWorkData() {}
	
	// When an action is encountered and the string is parsed.
	void handleAction(tokenType actionToken) {
		if(state == vInitialized || state == vCompleted) 
			throw std::runtime_error("Duplicate action key.");
		else {
			// Initialize event object.
			EventFieldHandler::createEventObject(actionToken, eventField);
			
			// Set the internal data and complete the object.
			if(state != vNone) {
				assert(state == vString || state == vInteger);
				if(state == vString) 
					EventFieldHandler::valueSetString(actionToken, 
						eventField, valueString.c_str(), valueString.length());
				else EventFieldHandler::valueSetInteger(
						actionToken, eventField, valueInteger);
				state = vCompleted;
			}
			
			// Store the action token and finish it when value is expected.
			else {
				state = vInitialized;
				storedActionToken = actionToken;
			}
		}
	}
	
	// When a value is encountered and it is string.
	void handleValueString(const char* str, size_t len) {
		if(state != vInitialized && state != vNone) 
			throw std::runtime_error("Duplicate value.");
		else if(state == vInitialized) {
			EventFieldHandler::valueSetString(
				storedActionToken, eventField, str, len);
			state = vCompleted;
		}
		else {
			valueString = std::string(str, len);
			state = vString;
		}
	}
	
	// When a value is encountered and it is integer.
	void handleValueInteger(uint64_t value) {
		if(state != vInitialized && state != vNone)
			throw std::runtime_error("Duplicate value.");
		else if(state == vInitialized) {
			EventFieldHandler::valueSetInteger(
				storedActionToken, eventField, value);
			state = vCompleted;
		}
		else {
			valueInteger = value;
			state = vInteger;
		}
	}
};

// The concrete event handler for hover event.
struct McDtHoverEventFieldHandler {
	typedef McDtUnion<McDtChatHoverInfo> eventFieldType;
	typedef const McDtChatParseToken* tokenType;
	
	// Create new hover event object.
	static void createEventObject(tokenType actionToken, eventFieldType& event) {
		if(actionToken == nullptr || actionToken -> acceptedContext != cpcHoverAct) 
				throw std::runtime_error("Unknown hover event action.");
		switch(actionToken -> tokenKey) {
			case keyActShowText: {
				event = McDtChatHoverShowText();
			} break;
			case keyActShowItem: {
				event = McDtChatHoverShowItem();
			} break;
			case keyActShowEntity: {
				event = McDtChatHoverShowEntity();
			} break;
			default: assert(false);
		}
	}
	
	// Handle integer value.
	static void valueSetInteger(tokenType actionToken, eventFieldType& event, uint64_t value) {
		throw std::runtime_error("Unexpected integer value.");
	}
	
	// Handle string value.
	static void valueSetString(tokenType actionToken, eventFieldType& event, 
			const char* str, size_t len) {
		if(actionToken == nullptr || actionToken -> acceptedContext != cpcHoverAct) 
				throw std::runtime_error("Unknown hover event action.");
		switch(actionToken -> tokenKey) {
			case keyActShowText: {
				event.asType<McDtChatHoverShowText>().text = mc::u16str(std::string(str, len));
			} break;
			case keyActShowItem: {
				event.asType<McDtChatHoverShowItem>().item = mc::u16str(std::string(str, len));
			} break;
			case keyActShowEntity: {
				event.asType<McDtChatHoverShowEntity>().entity = mc::u16str(std::string(str, len));
			} break;
			default: assert(false);
		}
	}
};


// The concrete event handler for click event.
struct McDtClickEventFieldHandler {
	typedef McDtUnion<McDtChatClickInfo> eventFieldType;
	typedef const McDtChatParseToken* tokenType;
	
	// Create new hover event object.
	static void createEventObject(const tokenType actionToken, eventFieldType& event) {
		if(actionToken == nullptr || actionToken -> acceptedContext != cpcClickAct) 
				throw std::runtime_error("Unknown click event action.");
		switch(actionToken -> tokenKey) {
			case keyActOpenUrl: {
				event = McDtChatClickOpenUrl();
			} break;
			case keyActRunCmd: {
				event = McDtChatClickRunCommand();
			} break;
			case keyActSuggestCmd: {
				event = McDtChatClickSuggestCommand();
			} break;
			case keyActChangePage: {
				event = McDtChatClickChangePage();
			} break;
			default: assert(false);
		}
	}
	
	// Handle integer value.
	static void valueSetInteger(const tokenType actionToken, eventFieldType& event, uint64_t value) {
		if(actionToken == nullptr || actionToken -> acceptedContext != cpcClickAct) 
				throw std::runtime_error("Unknown click event action.");
		switch(actionToken -> tokenKey) {
			case keyActOpenUrl:
			case keyActRunCmd:
			case keyActSuggestCmd: {
				throw std::runtime_error("Must provide string as value.");
			} break;
			case keyActChangePage: {
				event.asType<McDtChatClickChangePage>().pageNo = value;
			} break;
			default: assert(false);
		};
	}
	
	// Handle string value.
	static void valueSetString(const tokenType actionToken, eventFieldType& event, 
			const char* str, size_t len) {
		if(actionToken == nullptr || actionToken -> acceptedContext != cpcClickAct) 
				throw std::runtime_error("Unknown click event action.");
		switch(actionToken -> tokenKey) {
			case keyActOpenUrl: {
				event.asType<McDtChatClickOpenUrl>().url = mc::u16str(std::string(str, len));
			} break;
			case keyActRunCmd: {
				event.asType<McDtChatClickRunCommand>().command = mc::u16str(std::string(str, len));
			} break;
			case keyActSuggestCmd: {
				event.asType<McDtChatClickSuggestCommand>().command = mc::u16str(std::string(str, len));
			} break;
			case keyActChangePage: {
				throw std::runtime_error("Change page value cannot be string.");
			} break;
			default: assert(false);
		}
	}
};

// Implementation for the chat parsing handler.
class McDtChatParseHandler {
public:
	typedef McDtChatParseToken TokenType;
	
	typedef unsigned short ContextSetType;
	
	static inline McDtChatCompound& toCompound(const McDtJsonParseContext<>& ctx) {
		return *((McDtChatCompound*)ctx.workObject);
	}
	
	static inline McDtChatTraitScore& toScore(const McDtJsonParseContext<>& ctx) {
		return toCompound(ctx).content.asType<McDtChatTraitScore>();
	}
	
	static void expectedNull(const TokenType*, const McDtJsonParseContext<>&) { assert(false); }
	
	static void expectedInteger(const TokenType* tk, const McDtJsonParseContext<>& ctx, uint64_t value) {
		assert(tk != nullptr && tk -> tokenKey == keyValue);
		if(ctx.context == cpcClick) {
			((McDtChatEventWorkData<McDtClickEventFieldHandler>*)
				ctx.workData.get()) -> handleValueInteger(value);
		}
		else throw std::runtime_error("Integer value is not allowed for current context.");
	}
	
	static void expectedDouble(const TokenType*, const McDtJsonParseContext<>&, double) {	assert(false);	}
	
	static void expectedBool(const TokenType* tk, const McDtJsonParseContext<>& ctx, bool b) {
		assert(tk != nullptr);
		assert(ctx.context == cpcChatCompound);
		auto& compound = toCompound(ctx);
		
		// Forward to decoration keys.
		switch(tk -> tokenKey) {
			case keyBold: { compound.bold = b; } break;
			case keyItalic: { compound.italic = b; } break;
			case keyUnderlined: { compound.underlined = b; } break;
			case keyStrikeThrough: { compound.strikethrough = b; } break;
			case keyObfuscated: { compound.obfuscated = b; } break;
			default: assert(false);
		};
	}
	
	static constexpr const char* ambigiousTrait = "Ambigious chat trait encountered.";
	
	static void expectedString(const TokenType* tk, const McDtJsonParseContext<>& ctx, const char* name, size_t length) {
		if(tk != nullptr) {
			// No-previous-content guarantee.
			switch(tk -> tokenKey) {
				case keyText:
				case keyScore:
				case keyBind: {
					if(!toCompound(ctx).content.isNull()) 
						throw std::runtime_error(ambigiousTrait);
				} break;
				case keyTranslate: {
					if((!toCompound(ctx).content.isNull()) && (toCompound(ctx).content.ordinal()
						!= McDtChatTraitInfo::ordinalOf<McDtChatTraitTranslate>()))
						throw std::runtime_error(ambigiousTrait);
				} break;
				default: break;
			}
			
			// Perform the real work here.
			switch(tk -> tokenKey) {
				// Text decorations as string.
				case keyBold:
				case keyItalic:
				case keyUnderlined:
				case keyStrikeThrough:
				case keyObfuscated: {
					const McDtChatParseToken* cptk = McDtChatParseToken::lookup(name, length);
					if(cptk != nullptr && (cptk -> tokenKey == keyTrue)) expectedBool(tk, ctx, true);
					else if(cptk != nullptr && (cptk -> tokenKey == keyFalse)) expectedBool(tk, ctx, false);
					else throw std::runtime_error("Invalid value as text decoration, can only be 'true' or 'false'.");
				} break;
				
				// Text decorations as value.
				case keyColor: {
					const McDtChatColor* chatColor = McDtChatColor::lookup(name, length);
					if(chatColor == nullptr) throw std::runtime_error("Invalid chat color value.");
					toCompound(ctx).color = chatColor;
				} break;
				
				// The chat insertion.
				case keyInsertion: {
					toCompound(ctx).insertion = mc::u16str(std::string(name, length));
				} break;
				
				// The pure text chat trait.
				case keyText: {
					McDtChatTraitText text;
					text.text = mc::u16str(std::string(name, length));
					toCompound(ctx).content = std::move(text);
				} break;
				
				// The translate chat trait.
				case keyTranslate: {
					if(toCompound(ctx).content.isNull()) {
						McDtChatTraitTranslate translate;
						translate.translate = std::string(name, length);
						toCompound(ctx).content = std::move(translate);
					}
					else {
						toCompound(ctx).content.asType<McDtChatTraitTranslate>()
							.translate = std::string(name, length);
					}
				} break;
				
				// The keybind chat trait.
				case keyBind: {
					const McDtChatTraitKeybind* keybind = McDtChatTraitKeybind::lookup(name, length);
					if(keybind == nullptr) throw std::runtime_error("Invalid keybind value.");
					toCompound(ctx).content = keybind;
				} break;
				
				// Translation related to score component.
				case keyPlayerName: { 
					toScore(ctx).name = mc::u16str(std::string(name, length));
				} break;
				case keyObjective: {
					toScore(ctx).objective = std::move(mc::ustring<16>(mc::u16str(std::string(name, length))));
				} break;
				
				// The action expected.
				case keyAction: {
					assert(ctx.context == cpcHover || ctx.context == cpcClick);
					
					// Parse value token.
					const McDtChatParseToken* valueToken = 
						McDtChatParseToken::lookup(name, length);
					
					// Cast forward action.
					if(ctx.context == cpcHover) {
						((McDtChatEventWorkData<McDtHoverEventFieldHandler>*)
							ctx.workData.get()) -> handleAction(valueToken);
					}
					else {
						((McDtChatEventWorkData<McDtClickEventFieldHandler>*)
							ctx.workData.get()) -> handleAction(valueToken);
					}
				} break;
				
				// The value expected.
				case keyValue: {
					if(ctx.context == cpcScore) {
						toScore(ctx).value = mc::u16str(std::string(name, length));
					}
					else {
						assert(ctx.context == cpcHover || ctx.context == cpcClick);
						if(ctx.context == cpcHover) {
							((McDtChatEventWorkData<McDtHoverEventFieldHandler>*)
								ctx.workData.get()) -> handleValueString(name, length);
						}
						else {
							((McDtChatEventWorkData<McDtClickEventFieldHandler>*)
								ctx.workData.get()) -> handleValueString(name, length);
						}
					}
				} break;
				
				default: assert(false);
			}
		}
		else {
			// Append to the translate-with list.
			assert(ctx.context == cpcWith);
			toCompound(ctx).content.asType<McDtChatTraitTranslate>()
				.with.push_back(mc::u16str(std::string(name, length)));
		}
	}
	
	static void expectedCompound(const TokenType* tk, const McDtJsonParseContext<>& ctx, McDtJsonParseContext<>& octx) {
		// Judge whether it is genesis state now.
		if(tk == nullptr) switch(ctx.context) {
			// Right into the text compound parse.
			case cpcGenesis: {
				octx.context = cpcChatCompound;
				octx.workObject = ctx.workObject;
			} break;
			
			// Encounter the extra compound of the context.
			case cpcExtra: {
				octx.context = cpcChatCompound;
				auto& compound = toCompound(ctx);
				size_t lastIdx = compound.extra.size();
				
				// Inherit data from old compound.
				McDtChatCompound newCompound;
				newCompound.inheritStyle(compound);
				
				// Add to parent's extra and begin parse of child structure.
				compound.extra.push_back(std::move(newCompound));
				octx.workObject = (McDtChatCompound*)(&compound.extra[lastIdx]);
			} break;
		}
		else switch(tk -> tokenKey) {
			// The hover event compound.
			case keyHoverEvent: {
				octx.context = cpcHover;
				octx.workObject = ctx.workObject;
				octx.workData.reset(new McDtChatEventWorkData
					<McDtHoverEventFieldHandler>(toCompound(ctx).hoverEvent));
			} break;
			
			// The click event compound.
			case keyClickEvent: {
				octx.context = cpcClick;
				octx.workObject = ctx.workObject;
				octx.workData.reset(new McDtChatEventWorkData
					<McDtClickEventFieldHandler>(toCompound(ctx).clickEvent));
			} break;
			
			// The score compound.
			case keyScore: {
				octx.context = cpcScore;
				toCompound(ctx).content = McDtChatTraitScore();
				octx.workObject = ctx.workObject;
			} break;
			
			default: assert(false);	// Impossible cases.
		}
	}
	
	static void expectedArray(const TokenType* tk, const McDtJsonParseContext<>& ctx, McDtJsonParseContext<>& octx) {
		// The array could either be 'with' or 'extra', both are under cpcChatCompound.
		assert(tk != nullptr && ctx.context == cpcChatCompound);
		switch(tk -> tokenKey) {
			case keyExtra: {
				octx.context = cpcExtra;
				octx.contextAcceptableType = tCompound;
				octx.workObject = ctx.workObject;
			} break;
			case keyWith: {
				octx.context = cpcWith;
				octx.contextAcceptableType = tString;
				octx.workObject = ctx.workObject;
			} break;
			
			default: assert(false); // Impossible case.
		}
	}
	
	static const TokenType* lookup(const char* name, size_t length) {
		return McDtChatParseToken::lookup(name, length);
	}
};

// Implementation for the chat parsing method.
void McIoReadChatCompound(McIoInputStream& inputStream, 
	McDtChatCompound& compound, size_t expectedSize, bool tolerant) {
	
	// Push the genesis state into the driver stack.
	McDtJsonParseContext<> genesis;
	genesis.context = cpcGenesis;
	genesis.contextAcceptableType = tCompound;
	genesis.workObject = &compound;
	
	// Initialize parse related objects.
	McDtJsonParseDriver<McDtChatParseHandler> driver(std::move(genesis), tolerant);
	rapidjson::Reader jreader;
#if 0
	// Don't force insitu parsing.
	McIoJsonInputStream jinputStream(inputStream, expectedSize);
#else
	// Enforce insitu parsing.
	std::vector<char> data(expectedSize);
	inputStream.read(data.data(), expectedSize);
	rapidjson::GenericInsituStringStream<rapidjson::UTF8<char>> jinputStream(data.data());
#endif
	
	// Fire parse process.
	if(!jreader.Parse(jinputStream, driver)) 
		throw std::runtime_error(std::string("Error parsing json at index ")
			+ std::to_string(jreader.GetErrorOffset()) + std::string("."));
}

void McIoWriteChatCompound(McIoOutputStream& outputStream,
			const McDtChatCompound& compound) {
	
	// Not implemented.
}