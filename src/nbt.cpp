/**
 * @file nbt.cpp
 * @brief Implementation for nbt related utilities.
 * @author Haoran Luo
 *
 * For interface specification, please refer to the corresponding header.
 * @see libminecraft/nbt.hpp
 */
#include "libminecraft/nbt.hpp"
#include "libminecraft/iobase.hpp"
#include "libminecraft/stream.hpp"
#include <cassert>
#include <type_traits>
#include <codecvt>
#include <locale>
#include <list>

// The invalid nbt tag type message.
static const char* invalidNbtTagType = "Expected invalid nbt tag type.";

// Implementation for McIoReadNbtCompound.
void McIoReadNbtCompound(McIoInputStream& inputStream, mc::nbtcompound& compound) {
	// Just perform reading until an empty item occurs.
	while(true) {
		mc::nbtitem ioItem; inputStream >> ioItem;
		auto& item = (McDtNbtItem&)ioItem;
		auto itemKey = (const std::u16string&)item.first;
		
		// Append to the compound list.
		if(item.second.isNull()) break;
		else (McDtNbtPayload&)(compound[itemKey]) = std::move(item.second);
	}
}

// To be used in McIoReadNbtList to eliminate switch and #define.
struct McIoNbtTagListItemRead {
	/**
	 * @brief The normal read parser for nbt list types.
	 * @param[inout] inputStream the target input stream for reading.
	 * @param[out] list the element to read data into.
	 * @param[in] listLength the previously known length of list.
	 */
	template<typename V> static inline void perform(
		McIoInputStream& inputStream, McDtNbtList& list, int listLength) {
		std::vector<V> listVector;
		
		if(listLength > 0) {
			listVector.reserve(listLength);
			for(size_t i = 0; i < listLength; ++ i) {
				V data; inputStream >> data;
				listVector.push_back(data);
			}
		}
		mc::nbtlist dataList(listVector);
		list.swap(dataList);
	}
};

/// Specialization for McDtNbtList.
template<> inline void 
McIoNbtTagListItemRead::perform<McDtNbtList>(
	McIoInputStream& inputStream, McDtNbtList& list, int listLength) {
	std::vector<mc::nbtlist> listVector;
	if(listLength > 0) {
		listVector.reserve(listLength);
		for(size_t i = 0; i < listLength; ++ i) {
			mc::nbtlist sublist; 
			McIoReadNbtList(inputStream, sublist);
			listVector.push_back(std::move(sublist));
		}
	}
	mc::nbtlist aggregateList(listVector);
	list.swap(aggregateList);
}

/// Specialization for McDtNbtCompound.
template<> inline void 
McIoNbtTagListItemRead::perform<McDtNbtCompound>(
	McIoInputStream& inputStream, McDtNbtList& list, int listLength) {
	std::vector<mc::nbtcompound> listVector;
	if(listLength > 0) {
		listVector.reserve(listLength);
		for(size_t i = 0; i < listLength; ++ i) {
			mc::nbtcompound compound;
			McIoReadNbtCompound(inputStream, compound);
			listVector.push_back(std::move(compound));
		}
	}
	mc::nbtlist compoundList(listVector);
	list.swap(compoundList);
}

// Implementation for McIoReadNbtList.
void McIoReadNbtList(McIoInputStream& inputStream, mc::nbtlist& list) {
	mc::s8 listType; inputStream >> listType;
	mc::s32 listLength; inputStream >> listLength;
	
	// Eliminate malformed cases.
	if(listType < 0 || listType > 12)
		throw std::runtime_error(invalidNbtTagType);
	else if(listType == 0 && listLength > 0)
		throw std::runtime_error(invalidNbtTagType);
	else if(listType == 0) return;
	
	// Begin reading of the list.
	mc::nbtinfo.userByOrdinal<McIoNbtTagListItemRead,
			McIoInputStream&, mc::nbtlist&, mc::s32&>(
			listType - 1, inputStream, list, listLength);
}

// To be used in mc::nbt::read() to eliminate switch and #define.
struct McIoNbtTagItemRead {
	/**
	 * @brief The normal read parser for nbt types.
	 * @param[inout] inputStream the target input stream for reading.
	 * @param[out] payload the union to write read data to.
	 */
	template<typename V> static inline void perform(
		McIoInputStream& inputStream, McDtNbtPayload& payload) {
		V dataObject; inputStream >> dataObject;
		payload = std::move(dataObject);
	}
};

/// Specialization for McDtNbtList.
template<> inline void 
McIoNbtTagItemRead::perform<McDtNbtList>(
	McIoInputStream& inputStream, McDtNbtPayload& payload) {
	mc::nbtlist list;
	McIoReadNbtList(inputStream, list);
	payload = std::move(list);
}

/// Specialization for McDtNbtCompound.
template<> inline void 
McIoNbtTagItemRead::perform<McDtNbtCompound>(
	McIoInputStream& inputStream, McDtNbtPayload& payload) {
	mc::nbtcompound compound;
	McIoReadNbtCompound(inputStream, compound);
	payload = std::move(compound);
}

// Implementation for direct nbt I/O methods.
template<> McIoInputStream&
mc::nbtitem::read(McIoInputStream& inputStream) {
	// Judge the tag type first, and attempt to convert the internal object 
	// to specified ordinal. If the ordinal is invalid, an exception will
	// be thrown out.
	mc::s8 tagType; inputStream >> tagType;
	
	// The nbt null will contain nothing behind (even the compound name, so 
	// just do nothing in this case.
	if(tagType == 0) return inputStream;
	else if(tagType < 0 || tagType > 12)
		throw std::runtime_error(invalidNbtTagType);
	else tagType = tagType - 1;	// Bridge the gap between nbt ordinal and our ordinal.
	
	// Then read the tag of the nbt item.
	inputStream >> data.first;
	
	// Depending on the tag type, perform reading or deeper processing of the data.
	mc::nbtinfo.userByOrdinal<McIoNbtTagItemRead, McIoInputStream&, 
			McDtNbtPayload&>(tagType, inputStream, data.second);
	return inputStream;
}

// Skip normal elements in McIoSkipElement.
struct McIoNbtTagItemSkip {
	/**
	 * @brief The normal skipping method for nbt types.
	 * @param[inout] inputStream the target input stream for skipping.
	 */
	template<typename V> static inline void
	perform(McIoInputStream& inputStream) {
		static_assert(std::is_fundamental<typename V::type>::value,
			"Only primitive type could be specified to the skip!");
		inputStream.skip(sizeof(V));
	}
};

// The specialization for skipping arrays.
template<typename V>
inline void McIoNbtArraySkip(McIoInputStream& inputStream) {
	mc::s32 arrayLength; inputStream >> arrayLength;
	if(arrayLength <= 0) return;
	inputStream.skip(arrayLength * sizeof(V));
}

template<> inline void McIoNbtTagItemSkip
::perform<mc::array<mc::s8, mc::s32>>(McIoInputStream& inputStream) 
{	McIoNbtArraySkip<mc::s8>(inputStream);	}

template<> inline void McIoNbtTagItemSkip
::perform<mc::array<mc::s32, mc::s32>>(McIoInputStream& inputStream) 
{	McIoNbtArraySkip<mc::s32>(inputStream);	}

template<> inline void McIoNbtTagItemSkip
::perform<mc::array<mc::s64, mc::s32>>(McIoInputStream& inputStream) 
{	McIoNbtArraySkip<mc::s64>(inputStream);	}
	
// The specialization for skipping mc::nbtlist.
template<> inline void McIoNbtTagItemSkip
::perform<mc::nbtlist>(McIoInputStream& inputStream) {

	// Retrieve the the list essentials.
	mc::s8 tagType; inputStream >> tagType;
	mc::s32 listLength; inputStream >> listLength;
	if(tagType == 0 && listLength > 0) 
		throw std::runtime_error(invalidNbtTagType);
	else if(tagType == 0 || listLength <= 0) return;
	else tagType = tagType - 1;
	
	// See whether the underlying type is primitive.
	static const int elementSizes[] = {
		sizeof(mc::s8), sizeof(mc::s16), 
		sizeof(mc::s32), sizeof(mc::s64),
		sizeof(mc::f32), sizeof(mc::f64) };
	int size = tagType < (sizeof(elementSizes) / sizeof(int))?
				elementSizes[tagType] : 0;
	
	// Perform actual skipping.
	if(size == 0) for(size_t i = 0; i < listLength; ++ i)
		McIoSkipNbtElement(inputStream, tagType);
	else inputStream.skip(size * listLength);
}

// The specialization for skipping mc::nbtcompounds.
template<> inline void McIoNbtTagItemSkip
::perform<mc::nbtcompound>(McIoInputStream& inputStream) {
	
	while(true) {
		// Parse the type of the tag.
		mc::s8 tagType; inputStream >> tagType;
		if(tagType == 0) break;
		else if(tagType < 0 || tagType > 12)
			throw std::runtime_error(invalidNbtTagType);
		else tagType = tagType - 1;
		
		// Skip according to the tag.
		McIoSkipNbtElement(inputStream, tagType);
	}
}

// The specialization for skipping mc::jstring.
template<> inline void McIoNbtTagItemSkip
::perform<mc::jstring>(McIoInputStream& inputStream) {
	// Retrieve the utf-8 length.
	mc::u16 stringLength; inputStream >> stringLength;
	
	// Just skip the length, don't convert to utf-16.
	if(stringLength > 0) inputStream.skip(stringLength);
}

// Implementation for the skipping method.
void McIoSkipNbtElement(McIoInputStream& inputStream, mc::s8 tag) {
	mc::nbtinfo.userByOrdinal<McIoNbtTagItemSkip, McIoInputStream&>
		(tag, inputStream);
}

// The inlined place into ignored tag function.
// The inputStream's current position might either be after the name or 
// after the name length, and depending on the position, we skip up to 
// corresponding bytes.
inline void McIoSaxNbtCompoundPlaceIgnored(McDtNbtCompound* ignoredTag,
	mc::s8 tagType, size_t nameLength, const char* name, 
	McIoMarkableStream& inputStream) {
	
	// See whether the user structure is present.
	if(ignoredTag != nullptr) {
		// We have to place the struct into the compound.
		std::u16string tagName;
		
		// The tag string has been read, so convert the read buffer.
		if(name != nullptr) tagName = std::move(std::wstring_convert<
			std::codecvt_utf8_utf16<char16_t>, char16_t>()
			.from_bytes(std::string(name, nameLength)));
			
		// The tag string has not been read, so utilize the iobase method.
		else McIoReadUtf16String(inputStream, nameLength, tagName);
		
		// Read and place the payload behind.
		McDtNbtPayload payload;
		mc::nbtinfo.userByOrdinal<McIoNbtTagItemRead, McIoInputStream&, 
			McDtNbtPayload&>(tagType, inputStream, payload);
		(*ignoredTag)[tagName] = std::move(payload);
	}
	else {
		// We will just effectively skip it.
		if(name == nullptr) inputStream.skip(nameLength);
		McIoSkipNbtElement(inputStream, tagType);
	}
}

// Check whether all prerequisites of a sax action has met.
inline bool McIoSaxActionAllMet(std::vector<bool>& present, 
		const McIoNbtCompoundSaxAction& saxAction) {
	bool allPrerequisitesMet = true;
	for(size_t i = 0; i < saxAction.numPrerequisites; ++ i) {
		if(!present[saxAction.prerequisites[i]])
			{	allPrerequisitesMet = false; break;	}
	}
	return allPrerequisitesMet;
}

// Implementation for McIoSaxNbtCompound().
void McIoSaxNbtCompound(McIoMarkableStream& inputStream, void* data, void* ud,
	int (*dictionary)(size_t tagLength, const char* tagName), size_t numSaxActions, 
	const McIoNbtCompoundSaxAction* const saxActions, McDtNbtCompound* ignoredTag) {
	
	std::vector<bool> present(numSaxActions);
	size_t numPresents = 0;
	struct McIoSaxTagMarkItem {
		size_t action;							///< The index of the action.
		std::unique_ptr<McIoStreamMark> mark;	///< The mark of the stream.
	};
	std::list<McIoSaxTagMarkItem> marks;
	
	// Loop until has encountered a tag end.
	while(true) {
		// Parse the type and tag name first.
		mc::s8 type; inputStream >> type;
		if(type == 0) break;	// Tag end encounters.
		else if(type < 0 || type > 12) 
			throw std::runtime_error(invalidNbtTagType);
		else type = type - 1;
		mc::u16 tagLength; inputStream >> tagLength;
		
		// The tag will never be matched, so just place it in the ignored tags 
		// or simply skip it.
		if(tagLength >= McIoSaxMaxNbtTagNameLength) {
			McIoSaxNbtCompoundPlaceIgnored(ignoredTag, type, 
				tagLength, nullptr, inputStream);
			continue;
		}
		
		// The name length is valid, so read it into a buffer and look up the 
		// dictionary.
		char nameBuffer[McIoSaxMaxNbtTagNameLength + 1];
		inputStream.read(nameBuffer, tagLength);
		int entry = dictionary(tagLength, nameBuffer);
		
		// If the entry is not (or will not be) favoured, so just place it outside.
		if(entry < 0 || entry >= numSaxActions) {
			McIoSaxNbtCompoundPlaceIgnored(ignoredTag, 
				type, tagLength, nameBuffer, inputStream);
			continue;
		}
		
		// Help users to find whether they have initialized the actions correctly.
		const McIoNbtCompoundSaxAction& saxAction = saxActions[entry];
		assert(saxAction.expectedType <= 25);
		
		// If mismatched (not typed list), also place it outside.
		if(saxAction.expectedType <= 12 && saxAction.expectedType != type) {
			McIoSaxNbtCompoundPlaceIgnored(ignoredTag, 
				type, tagLength, nameBuffer, inputStream);
			continue;
		}
		
		// If mismatched (typed list), also place it outside.
		if(saxAction.expectedType > 12) {
			std::unique_ptr<McIoStreamMark> mark;
			if(ignoredTag != nullptr) mark = inputStream.mark();
			
			mc::s8 componentType; inputStream >> componentType;
			if(componentType > 0) {	// Except for nbt end could match any type.
				if(componentType + 12 != saxActions[entry].expectedType) {
					mark.reset();
					McIoSaxNbtCompoundPlaceIgnored(ignoredTag, 
						type, tagLength, nameBuffer, inputStream);
					continue;
				}
			}
		}
		
		// Depending on whether prerequisite is met, begin processing it or place
		// it on the mark list.
		if(McIoSaxActionAllMet(present, saxAction)) {
			present[entry] = true; ++ numPresents;
			assert(saxAction.tagPresent != nullptr);
			saxAction.tagPresent(inputStream, data, ud);
		}
		else {
			std::unique_ptr<McIoStreamMark> mark = inputStream.mark();
			McIoSaxTagMarkItem item;
			item.action = entry; 
			item.mark.swap(mark);
			marks.push_back(std::move(item));
		}
	}
	
	// There're entries that requires to be resolved.
	if(!marks.empty()) {
		std::unique_ptr<McIoStreamMark> endCompoundMark = inputStream.mark();
		
		// Now lets have a look at the prerequisite-not-met entries.
		// All entries' prerequisite are assumed to be parsed, if there'are N remaining
		// entries, the worst case of deducing will be N pass full traversal.
		size_t maxPass = marks.size();
		for(size_t pass = 0; pass < maxPass && !marks.empty(); ++ pass) {
			
			for(auto iter = marks.begin(); iter != marks.end(); ) {
				const McIoNbtCompoundSaxAction& saxAction = saxActions[iter -> action];
				
				if(McIoSaxActionAllMet(present, saxAction)) {
					iter -> mark -> reset();
					assert(saxAction.tagPresent != nullptr);
					saxAction.tagPresent(inputStream, data, ud);
					present[iter -> action] = true; ++ numPresents;
					iter = marks.erase(iter);
				}
				else ++ iter;
			}
		}
		
		// Invoke the cannot resolve methods for those who fails to resolve.
		if(!marks.empty()) {
			for(auto iter = marks.begin(); iter != marks.end(); ++ iter) {
				if(saxActions[iter -> action].tagFailedResolve != nullptr) {
					saxActions[iter -> action].tagFailedResolve
						(inputStream, data, ud);
					present[iter -> action] = true; ++ numPresents;
				}
			}
		}
		
		// Reset the mark to the last of the stream.
		endCompoundMark = 0;
	}
	
	// Invoke those who have their not present method registered.
	if(numPresents < numSaxActions) for(size_t i = 0; i < numSaxActions; ++ i)
		if((!present[i]) && (saxActions[i].tagAbsent != nullptr))
			saxActions[i].tagAbsent(data, ud);
}