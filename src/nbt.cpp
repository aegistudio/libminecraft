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

// The invalid nbt tag type message.
static const char* invalidNbtTagType = "Expected invalid nbt tag type.";

// The declaration for the NbtList read method.
void McIoReadNbtList(McIoInputStream& inputStream, mc::nbtlist& list);

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
	typedef std::tuple<McDtNbtList*, int> dstBufferTuple;
	/**
	 * @brief The normal read parser for nbt list types.
	 * @param[out] dstBuffer must be set to the pointer to McDtNbtPayload.
	 * @param[inout] srcBuffer must be set to the pointer to McIoInputStream.
	 */
	template<typename V> static inline void perform(
		McIoInputStream& inputStream, McDtNbtList& list, int listLength) {
		std::vector<V> listVector;
		for(size_t i = 0; i < listLength; ++ i) {
			V data; inputStream >> data;
			listVector.push_back(data);
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
	for(size_t i = 0; i < listLength; ++ i) {
		mc::nbtlist sublist; 
		McIoReadNbtList(inputStream, sublist);
		listVector.push_back(std::move(sublist));
	}
	mc::nbtlist aggregateList(listVector);
	list.swap(aggregateList);
}

/// Specialization for McDtNbtCompound.
template<> inline void 
McIoNbtTagListItemRead::perform<McDtNbtCompound>(
	McIoInputStream& inputStream, McDtNbtList& list, int listLength) {
	std::vector<mc::nbtcompound> listVector;
	for(size_t i = 0; i < listLength; ++ i) {
		mc::nbtcompound compound;
		McIoReadNbtCompound(inputStream, compound);
		listVector.push_back(std::move(compound));
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
	 * @param[out] dstBuffer must be set to the pointer to McDtNbtPayload.
	 * @param[inout] srcBuffer must be set to the pointer to McIoInputStream.
	 */
	template<typename V> static inline  void perform(
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
}