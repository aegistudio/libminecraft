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

// The event work data that is useful under chat click event and chat hover event.
struct McDtChatEventWorkData : public McDtJsonWorkData {
	// Containing the value when it is an utf-8 string.
	std::string valueString;
	
	// Containing the value when it is an integer.
	uint64_t valueInteger;
	
	// Track state for the working data, 0 for not specified,
	// 1 for value string, 2 for value integer.
	enum valueState {
		vNone = 0, vString = 1, vInteger = 2
	};
	valueState state;
	
	// Initialize the event work data.
	McDtChatEventWorkData(): valueString(), valueInteger(0), state(vNone) {}
	
	// Destruct the event work data.
	virtual ~McDtChatEventWorkData() {}
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
	
	static void expectedInteger(const TokenType*, const McDtJsonParseContext<>&, uint64_t) {}
	
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
					McDtChatTraitKeybind keybind;
					keybind.key = nullptr /* Translate to keybind please */;
				} break;
				
				// Translation related to score component.
				case keyPlayerName: { 
					toScore(ctx).name = mc::u16str(std::string(name, length));
				} break;
				case keyObjective: {
					toScore(ctx).objective = mc::u16str(std::string(name, length));
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
				octx.workData.reset(new McDtChatEventWorkData);
			} break;
			
			// The click event compound.
			case keyClickEvent: {
				octx.context = cpcClick;
				octx.workObject = ctx.workObject;
				octx.workData.reset(new McDtChatEventWorkData);
			} break;
			
			// The score compound.
			case keyScore: {
				octx.context = cpcScore;
				auto& compound = toCompound(ctx);
				compound.content = McDtChatTraitScore();
				octx.workObject = ctx.workObject;
			} break;
			
			default: assert(false);	// Impossible cases.
		}
	}
	
	static void expectedArray(const TokenType*, const McDtJsonParseContext<>&, McDtJsonParseContext<>&) {}
	
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
	McIoJsonInputStream jinputStream(inputStream, expectedSize);
	rapidjson::Reader jreader;
	
	// Fire parse process.
	if(!jreader.Parse(jinputStream, driver)) 
		throw std::runtime_error(std::string("Error parsing json at index ")
			+ std::to_string(jreader.GetErrorOffset()) + std::string("."));
}

void McIoWriteChatCompound(McIoOutputStream& outputStream,
			const McDtChatCompound& compound) {
	
	// Not implemented.
}