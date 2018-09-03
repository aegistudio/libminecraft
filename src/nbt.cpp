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
		case list.info.ordinalOf<dataType>() + 1: {\
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
		case list.info.ordinalOf<mc::nbtlist>() + 1: {
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
		case list.info.ordinalOf<mc::nbtcompound>() + 1: {
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
	
	// Depending on the data, perform reading or deeper processing of the 
	// data.
	switch(tagType) {
#define McDtNbtReadSwitch(dataType)\
		case data.second.info.ordinalOf<dataType>(): {\
			dataType dataObject; inputStream >> dataObject;	\
			data.second = dataObject;						\
		} break;
		
		// The non-composite data types.
		McDtNbtReadSwitch(mc::s8);
		McDtNbtReadSwitch(mc::s16);
		McDtNbtReadSwitch(mc::s32);
		McDtNbtReadSwitch(mc::s64);
		McDtNbtReadSwitch(mc::f32);
		McDtNbtReadSwitch(mc::f64);
		McDtNbtReadSwitch(mc::jstring);
		McDtNbtReadSwitch(mc::nbtintarray<mc::s8>);
		McDtNbtReadSwitch(mc::nbtintarray<mc::s32>);
		McDtNbtReadSwitch(mc::nbtintarray<mc::s64>);
		
		// The compound data type.
		case data.second.info.ordinalOf<mc::nbtcompound>(): {
			mc::nbtcompound compound;
			McIoReadNbtCompound(inputStream, compound);
			data.second = std::move(compound);
		}; break;
		
		// The list data type.
		case data.second.info.ordinalOf<mc::nbtlist>(): {
			mc::nbtlist list;
			McIoReadNbtList(inputStream, list);
			data.second = std::move(list);
		}; break;
	}
}