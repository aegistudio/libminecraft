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

// Implementation for McIoReadNbtList.
void McIoReadNbtList(McIoInputStream& inputStream, mc::nbtlist& list) {
	mc::s8 listType; inputStream >> listType;
	mc::s32 listLength; inputStream >> listLength;
	
	// Eliminate malformed cases.
	if(listType < 0 || listType > 12)
		throw std::runtime_error(invalidNbtTagType);
	else if(listType == 0 && listLength != 0)
		throw std::runtime_error(invalidNbtTagType);
	
	// Begin reading of the list.
	switch(listType) {
#define McNbtReadListSwitch(dataType)\
		case mc::nbtinfo.ordinalOf<dataType>() + 1: {\
			std::vector<dataType> listVector;\
			for(size_t i = 0; i < listLength; ++ i) {\
				dataType data; inputStream >> data;\
				listVector.push_back(data);\
			}\
			mc::nbtlist dataList(listVector);\
			list.swap(dataList);\
		}; break;
		
		McNbtReadListSwitch(mc::s8);
		McNbtReadListSwitch(mc::s16);
		McNbtReadListSwitch(mc::s32);
		McNbtReadListSwitch(mc::s64);
		McNbtReadListSwitch(mc::f32);
		McNbtReadListSwitch(mc::f64);
		McNbtReadListSwitch(mc::jstring);
		McNbtReadListSwitch(mc::nbtintarray<mc::s8>);
		McNbtReadListSwitch(mc::nbtintarray<mc::s32>);
		McNbtReadListSwitch(mc::nbtintarray<mc::s64>);
		
		// Special case for nbt list.
		case mc::nbtinfo.ordinalOf<mc::nbtlist>() + 1: {
			std::vector<mc::nbtlist> listVector;
			for(size_t i = 0; i < listLength; ++ i) {
				mc::nbtlist sublist; 
				McIoReadNbtList(inputStream, sublist);
				listVector.push_back(std::move(sublist));
			}
			mc::nbtlist aggregateList(listVector);
			list.swap(aggregateList);
		}; break;
		
		// Special case for nbt compound.
		case mc::nbtinfo.ordinalOf<mc::nbtcompound>() + 1: {
			std::vector<mc::nbtcompound> listVector;
			for(size_t i = 0; i < listLength; ++ i) {
				mc::nbtcompound compound;
				McIoReadNbtCompound(inputStream, compound);
				listVector.push_back(std::move(compound));
			}
			mc::nbtlist compoundList(listVector);
			list.swap(compoundList);
		}; break;
	}
}

// To be used by mc::nbtinfo.byOrdinal<V>(), and eliminate duplicate code.
struct McIoNbtTagItemRead {
	/**
	 * @brief The normal read parser for nbt compound types.
	 * @param[out] dstBuffer must be set to the pointer to McDtNbtPayload.
	 * @param[inout] srcBuffer must be set to the pointer to McIoInputStream.
	 * @param[out] valueValid unused.
	 */
	template<typename V> static inline  void performRead(
		McIoInputStream& inputStream, McDtNbtPayload& payload) {
		V dataObject; inputStream >> dataObject;
		payload = std::move(dataObject);
	}

	/**
	 * @brief The normal read parser for nbt compound types.
	 * @param[out] dstBuffer must be set to the pointer to McDtNbtPayload.
	 * @param[inout] srcBuffer must be set to the pointer to McIoInputStream.
	 * @param[out] valueValid unused.
	 */
	template<typename V> static inline void perform(
		bool& valueValid, char* dstBuffer, char* srcBuffer) {
		assert(srcBuffer != nullptr && dstBuffer != nullptr);
		
		performRead<V>(
			*reinterpret_cast<McIoInputStream*>(srcBuffer),
			*reinterpret_cast<McDtNbtPayload*>(dstBuffer));
	}
};

/// Specialization for McDtNbtList.
template<> inline void 
McIoNbtTagItemRead::performRead<McDtNbtList>(
	McIoInputStream& inputStream, McDtNbtPayload& payload) {
	mc::nbtlist list;
	McIoReadNbtList(inputStream, list);
	payload = std::move(list);
}

/// Specialization for McDtNbtCompound.
template<> inline void 
McIoNbtTagItemRead::performRead<McDtNbtCompound>(
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
	bool dummy = false;	// In place of valueValid.
	mc::nbtinfo.byOrdinal<McIoNbtTagItemRead>(tagType, 
		dummy, (char*)&(data.second), (char*)&inputStream);
}