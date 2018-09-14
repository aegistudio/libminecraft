#pragma once
/**
 * @file jsontoken.hpp
 * @brief Headers for json tokenizing related objects.
 * @author Haoran Luo
 *
 * This file specifies concepts related to json tokenizing and 
 * reconstructing. Other modules could build on the module to 
 * tokenize and reconstruct from the tokens.
 *
 * Any tokenizing starts from a json compound, and error will be 
 * generated if not so. A json compound or a json array is a 
 * context. Context specifies what is legal under a compound or 
 * an array.
 *
 * The legislature of a token depends on context and type. A token
 * may live under some context, and its value yields some type.
 * If the json fails to meet the legislature requirement, a parse
 * error will also be generated.
 *
 * A tokenize handler works on two parts: the object it is constructing
 * (called working objectg), and ledge it records current state on
 * (called working data). Both working object and working data should 
 * correctly manages its state when parse error occurs. The working data
 * is managed by the parse handler, however the working object is managed
 * elsewhere.
 */

#include "rapidjson/rapidjson.h"
#include "rapidjson/reader.h"
#include "libminecraft/stream.hpp"
#include <stack>
#include <sstream>
#include <memory>

/// Records what types are possible for json.
enum McDtJsonType {
	/// The value is not a type, and will always generate error if such 
	/// token is expected. 
	tUndefined = 0,

	/// The value could accept in null type.
	tNullable = 1,
	
	/// The value is boolean, means either true or false.
	tBoolean = 2,
	
	/// The value is integer, either Int, Uint, Int64 or Uint64 of rapidjson.
	tInteger = 4,
	
	/// The value is floating point.
	tFloat = 8,
	
	/// The value is string.
	tString = 16,
	
	/// The value is a compound type. Whenever a compound is encountered, a 
	/// context is generated for it. Make sure you configure one correctly.
	tCompound = 32,
	
	/// The value is a array type. Whenever an array is encountered a context
	/// is generated for it. Make sure you configure one correctly.
	tArray = 64,
};

/// Abstraction for the working data, which is managed by the handler.
struct McDtJsonWorkData { 
	/// Virtual destructor is mandatory for such situation.
	virtual ~McDtJsonWorkData() {}
};

/// Abstraction for a currently parse context.
template<typename ContextSetType = unsigned short>
struct McDtJsonParseContext {
	/// Current working context of the json.
	ContextSetType context;
	
	/// Current working data of the context.
	std::unique_ptr<McDtJsonWorkData> workData;
	
	/// Context permitted types, should only be used in array or genesis state.
	ContextSetType contextAcceptableType;
	
	/// Current working object of the context.
	void* workObject;
	
	// Initialize blank object.
	McDtJsonParseContext(): context(0), workData(), 
		contextAcceptableType(0), workObject(nullptr) {}
};

/// Abstraction templated handler.
template <typename HandlerType>
struct McDtJsonParseDriver : public rapidjson::BaseReaderHandler<
		rapidjson::UTF8<>, McDtJsonParseDriver<HandlerType>> {
	
	// Forward the type of the token.
	typedef typename HandlerType::TokenType TokenType;
	typedef typename HandlerType::ContextSetType ContextSetType;
	typedef McDtJsonParseContext<ContextSetType> ContextType;
	
	/// The stacked contexts.
	std::stack<ContextType> contexts;
	
	/// The currently working token.
	const TokenType* currentToken;
	
	/// Whether it is under tolerant mode of parsing. Unrecognized data will 
	/// be ignored on tolerant mode, but error will be thrown if not.
	bool tolerant;
	
	/// The counter to unrecognize data when it is greater than 0.
	/// Increase when it is greater than 0 and enter object or array.
	int ignoreCounter;
	
	McDtJsonParseDriver(ContextType&& context, bool tolerant = false): 
		contexts(), currentToken(nullptr), 
		tolerant(tolerant), ignoreCounter(0) {
			contexts.push(std::move(context));
		}
	
	/// Identifies whether the current k-v pair is accepted type.
	inline bool isAcceptedType(unsigned short type) {
		if(ignoreCounter > 0) return true;
		if(currentToken == nullptr) 
			return (contexts.top().contextAcceptableType & type) == type;
		if(((currentToken -> acceptedType) & type) != type) return false;
		return true;
	}
	
	bool Bool(bool b) {
		if(ignoreCounter > 0) return true;
		if(!isAcceptedType(tBoolean)) {
			if(tolerant) return true;
			std::stringstream errorMsg;
			errorMsg << "Unexpected json key-value pair: " << 
				((currentToken != nullptr)? currentToken -> name : "<?>")
				<< (b? " : true." : " : false.");
			throw std::runtime_error(errorMsg.str());
		}
		else {
			HandlerType::expectedBool(currentToken, contexts.top(), b);
			return true;
		}
	}
	
	/// When the handler driver encounters null.
	bool Null() {
		if(ignoreCounter > 0) return true;
		if(!isAcceptedType(tNullable)) {
			if(tolerant) return true;
			std::stringstream errorMsg;
			errorMsg << "Unexpected json key-value pair: " << 
				((currentToken != nullptr)? currentToken -> name : "<?>")
				<< " : null.";
			throw std::runtime_error(errorMsg.str());
		}
		else {
			HandlerType::expectedNull(currentToken, contexts.top());
			return true;
		}
	}
	
	/// When the handler driver encounters integers.
	inline bool handleInteger(uint64_t intValue) {
		if(ignoreCounter > 0) return true;
		if(!isAcceptedType(tInteger)) {
			if(tolerant) return true;
			std::stringstream errorMsg;
			errorMsg << "Unexpected json key-value pair: " <<
				((currentToken != nullptr)? currentToken -> name : "<?>")
				<< " : " << intValue << ".";
			throw std::runtime_error(errorMsg.str());
		}
		else {
			HandlerType::expectedInteger(currentToken, contexts.top(), intValue);
			return true;
		}
	}
	
	// Aggregate integer handling.
	bool Int(int i) { return handleInteger(i); }
	bool Uint(unsigned u) { return handleInteger(u); }
	bool Int64(int i64) { return handleInteger(i64); }
	bool Uint64(int u64) { return handleInteger(u64); }
	
	/// When the handler driver encounters double.
	bool Double(double d) {
		if(ignoreCounter > 0) return true;
		if(!isAcceptedType(tFloat)) {
			if(tolerant) return true;
			std::stringstream errorMsg;
			errorMsg << "Unexpected json key-value pair: " << 
				((currentToken != nullptr)? currentToken -> name : "<?>") 
				<< " : " << d << ".";
			throw std::runtime_error(errorMsg.str());
		}
		else {
			HandlerType::expectedDouble(currentToken, contexts.top(), d);
			return true;
		}
	}
	
	/// When the handler driver encounters a string.
	bool String(const char* str, rapidjson::SizeType length, bool notInsitu) {
		if(ignoreCounter > 0) return true;
		if(!isAcceptedType(tString)) {
			if(tolerant) return true;
			std::stringstream errorMsg;
			errorMsg << "Unexpected json key-value pair: " <<
				((currentToken != nullptr)? currentToken -> name : "<?>") 
				<< ":" << std::string(str, length) << ".";
			throw std::runtime_error(errorMsg.str());
		}
		else {
			HandlerType::expectedString(currentToken, contexts.top(), str, length);
			return true;
		}
	}
	
	/// When the handler driver encounters a compound.
	bool StartObject() {
		if(ignoreCounter > 0) { ++ ignoreCounter; return true; }
		if(!isAcceptedType(tCompound)) {
			if(tolerant) { ++ ignoreCounter; return true; }
			std::stringstream errorMsg;
			errorMsg << "Unexpected json key-value pair: " <<
				((currentToken != nullptr)? currentToken -> name : "<?>") 
				<< " : {<jsonObject>}.";
			throw std::runtime_error(errorMsg.str());
		}
		else {
			ContextType contextToPush;
			HandlerType::expectedCompound(currentToken, contexts.top(), contextToPush);
			contexts.push(std::move(contextToPush));
			currentToken = nullptr;
			return true;
		}
	}
	
	/// When the handler driver encounters an array.
	bool StartArray() {
		if(ignoreCounter > 0) { ++ ignoreCounter; return true; }
		if(!isAcceptedType(tArray)) {
			if(tolerant) { ++ ignoreCounter; return true; }
			std::stringstream errorMsg;
			errorMsg << "Unexpected json key-value pair: " <<
				((currentToken != nullptr)? currentToken -> name : "<?>") 
				<< " : [<jsonArray>].";
			throw std::runtime_error(errorMsg.str());
		}
		else {
			ContextType contextToPush;
			HandlerType::expectedArray(currentToken, contexts.top(), contextToPush);
			contexts.push(std::move(contextToPush));
			currentToken = nullptr;
			return true;
		}
	}
	
	/// When the handler driver exits a compound.
	bool EndObject(rapidjson::SizeType memberCount) {
		if(ignoreCounter > 0) -- ignoreCounter;
		else {
			contexts.pop();
			currentToken = nullptr;
		}
		return true;
	}
	
	/// When the handler driver exits an array.
	bool EndArray(rapidjson::SizeType elementCount) {
		if(ignoreCounter > 0) -- ignoreCounter;
		else {
			contexts.pop();
			currentToken = nullptr;
		}
		return true;
	}
	
	/// Parse the key for the handler driver.
	bool Key(const char* str, rapidjson::SizeType length, bool copy) {
		if(ignoreCounter > 0) return true;
		
		// Perform token lookup.
		currentToken = HandlerType::lookup(str, length);
		if(currentToken != nullptr && 
			(currentToken -> acceptedContext & contexts.top().context) == 0)
			currentToken = nullptr;

		// Handle token parse failure.
		if(currentToken == nullptr) {
			if(tolerant) return true;
			std::stringstream errorMsg;
			errorMsg << "Unexpected json key encountered: " << 
				std::string(str, length) << ".";
			throw std::runtime_error(errorMsg.str());
		}
		else return true;
	}
};


/**
 * The Conceptual type for handler (You can copy and paste to implement your 
 * own handler type):
 */
class /*concept*/ __ConceptHandlerType {
	__ConceptHandlerType() = delete;
public:
	typedef int TokenType;
	
	static void expectedNull(const TokenType*, const McDtJsonParseContext<>&) {}
	
	static void expectedBool(const TokenType*, const McDtJsonParseContext<>&, bool) {}
	
	static void expectedInteger(const TokenType*, const McDtJsonParseContext<>&, uint64_t) {}
	
	static void expectedDouble(const TokenType*, const McDtJsonParseContext<>&, double) {}
	
	static void expectedString(const TokenType*, const McDtJsonParseContext<>&, const char*, size_t) {}
	
	static void expectedCompound(const TokenType*, const McDtJsonParseContext<>&, McDtJsonParseContext<>&) {}
	
	static void expectedArray(const TokenType*, const McDtJsonParseContext<>&, McDtJsonParseContext<>&) {}
	
	static const TokenType* lookup(const char*, size_t) { return nullptr; }
};

/// Abstraction for input stream adapting (for rapidjson).
class McIoJsonInputStream {
	/// The wrapped input stream.
	McIoInputStream& wrappedInputStream;
	
	/// The most length of the current json portion.
	size_t maxLength;
	
	/// The current position of the json.
	size_t current;
	
	// The character that would be peeked for the json stream.
	char peeking;
public:
	// The default constructor.
	McIoJsonInputStream(McIoInputStream& wrappedInputStream, size_t maxLength):
		wrappedInputStream(wrappedInputStream), maxLength(maxLength), current(0), peeking('\0') {
			
			if(maxLength > 0) wrappedInputStream.read(&peeking, 1);
		}

	/// Stream type of json input stream.
	typedef char Ch;
	
	/// Peek the current character from stream.
	Ch Peek() const {	return peeking;		}
	
	/// Take the first character from the stream.
	Ch Take() {
		if(current < maxLength) {
			Ch temp = peeking;
			wrappedInputStream.read(&peeking, 1);
			++ current;
			return temp;
		}
		else return '\0';
	}
	
	/// Tell the current position of the stream.
	size_t Tell() {
		return current;
	}
	
	// Adapt invalid methods for input stream.
	Ch* PutBegin() { throw std::logic_error("Not implemented."); }
	void Put(Ch) { throw std::logic_error("Not implemented."); }
	void Flush() { throw std::logic_error("Not implemented."); }
	size_t PutEnd(Ch* begin) { throw std::logic_error("Not implemented."); }
};