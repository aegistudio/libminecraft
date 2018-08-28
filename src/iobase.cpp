/**
 * @file iobase.cpp
 * @brief Implementation for iobase.
 * @author Haoran Luo
 *
 * For interface specification, please refer to the corresponding header.
 * @see libminecraft/iobase.hpp
 */
#include "libminecraft/iobase.hpp"
#include <netinet/in.h>
#include <stdexcept>
#include <vector>

#ifdef _MSC_VER
// The microsoft has already implemented ntohll() and htonll().
#include <winsock2.h>
#else
// Use be64toh() as ntohll() and htobe64() as htonll().
#include <endian.h>
inline uint64_t ntohll(uint64_t v) { return be64toh(v); }
inline uint64_t htonll(uint64_t v) { return htobe64(v); }
#endif

// The general output function for fixed flavour data, including
// a host-network conversion function and an output call.
template<typename T, T (Hton)(T)>
inline McIoOutputStream& out(McIoOutputStream& outputStream, 
	const McDtDataType<T, McDtFlavourFixed>& data) {
	
	T networkData = Hton(data);
	outputStream.write((const char*)&networkData, sizeof(networkData));
	return outputStream;
}

// The general input function for fixed flavour data, including 
// a host-network conversion and an input call.
template<typename T, T (Ntoh)(T)>
inline McIoInputStream& in(McIoInputStream& inputStream,
	McDtDataType<T, McDtFlavourFixed>& data) {
	
	T networkData;
	inputStream.read((char*)&networkData, sizeof(networkData));
	data = Ntoh(networkData);
	return inputStream;
}

// The host-network conversion that do nothing, suitable for single byte.
template<typename T>
inline T emptyconv(T v) { return v; }

// Instantiation of mc::s8's I/O methods.
template<> McIoOutputStream& 
mc::s8::write(McIoOutputStream& outputStream) const {
	return out<int8_t, emptyconv<int8_t> >(outputStream, *this);
}

template<> McIoInputStream&
mc::s8::read(McIoInputStream& inputStream) {
	return in<int8_t, emptyconv<int8_t> >(inputStream, *this);
}

// Instantiation of mc::u8's I/O methods.
template<> McIoOutputStream& 
mc::u8::write(McIoOutputStream& outputStream) const {
	return out<uint8_t, emptyconv<uint8_t> >(outputStream, *this);
}

template<> McIoInputStream& 
mc::u8::read(McIoInputStream& inputStream) {
	return in<uint8_t, emptyconv<uint8_t> >(inputStream, *this);
}

// The host-network conversion that involves a concrete function.
template<typename T, typename V, V (nhconv)(V)> 
inline T concreteconv(T v) {
	T w;
	*((V*)&w) = nhconv(*((V*)&v));
	return w;
}

// Instantiation of mc::s16's I/O methods.
template<> McIoOutputStream& 
mc::s16::write(McIoOutputStream& outputStream) const {
	return out<int16_t, concreteconv<int16_t, uint16_t, htons> >(outputStream, *this);
}

template<> McIoInputStream& 
mc::s16::read(McIoInputStream& inputStream) {
	return in<int16_t, concreteconv<int16_t, uint16_t, ntohs> >(inputStream, *this);
}

// Instantiation of mc::u16's I/O methods.
template<> McIoOutputStream& 
mc::u16::write(McIoOutputStream& outputStream) const {
	return out<uint16_t, concreteconv<uint16_t, uint16_t, htons> >(outputStream, *this);
}

template<> McIoInputStream& 
mc::u16::read(McIoInputStream& inputStream) {
	return in<uint16_t, concreteconv<uint16_t, uint16_t, ntohs> >(inputStream, *this);
}

// Instantiation of mc::s32's I/O methods.
template<> McIoOutputStream& 
mc::s32::write(McIoOutputStream& outputStream) const {
	return out<int32_t, concreteconv<int32_t, uint32_t, htonl> >(outputStream, *this);
}

template<> McIoInputStream& 
mc::s32::read(McIoInputStream& inputStream) {
	return in<int32_t, concreteconv<int32_t, uint32_t, ntohl> >(inputStream, *this);
}

// Instantiation of mc::u32's I/O methods.
template<> McIoOutputStream& 
mc::u32::write(McIoOutputStream& outputStream) const {
	return out<uint32_t, concreteconv<uint32_t, uint32_t, htonl> >(outputStream, *this);
}

template<> McIoInputStream& 
mc::u32::read(McIoInputStream& inputStream) {
	return in<uint32_t, concreteconv<uint32_t, uint32_t, ntohl> >(inputStream, *this);
}

// Instantiation of mc::s64's I/O methods.
template<> McIoOutputStream& 
mc::s64::write(McIoOutputStream& outputStream) const {
	return out<int64_t, concreteconv<int64_t, uint64_t, htonll> >(outputStream, *this);
}

template<> McIoInputStream& 
mc::s64::read(McIoInputStream& inputStream) {
	return in<int64_t, concreteconv<int64_t, uint64_t, ntohll> >(inputStream, *this);
}

// Instantiation of mc::u64's I/O methods.
template<> McIoOutputStream& 
mc::u64::write(McIoOutputStream& outputStream) const {
	return out<uint64_t, concreteconv<uint64_t, uint64_t, htonll> >(outputStream, *this);
}

template<> McIoInputStream& 
mc::u64::read(McIoInputStream& inputStream) {
	return in<uint64_t, concreteconv<uint64_t, uint64_t, ntohll> >(inputStream, *this);
}

// The variant length integer's output method.
template<typename T, int maxLength, int msbmax>
inline McIoOutputStream& out(McIoOutputStream& outputStream, 
	const McDtDataType<T, McDtFlavourVariant>& data) {
	
	T value = data;
	for(size_t i = 0; i < maxLength; ++ i) {
		// Retrieve current byte.
		unsigned char current = (char)(((int)value) & 0x07f);
		value = value >> 7;
		
		// Analyze and write out.
		if(value != 0) current |= (char)0x080;
		if(i == maxLength - 1) current &= msbmax;
		outputStream.write((char*)&current, 1);
		if(value == 0) break;
	}
	return outputStream;
}

// The variant length integer's output method.
template<typename T, int maxLength, int msbmax>
inline McIoInputStream& in(McIoInputStream& inputStream, 
	McDtDataType<T, McDtFlavourVariant>& data) {
	
	T value = 0;
	for(size_t i = 0; i < maxLength; ++ i) {
		// Retrieve current byte.
		unsigned char current;
		inputStream.read((char*)&current, 1);
		value |= ((int)current & 0x07f) << (i * 7);
		
		// Analyze and determine whether to continue.
		if(i == maxLength - 1) {
			if(current > msbmax) throw std::runtime_error(
				"Malformed variant integer value.");
		}
		else if((current & 0x80) == 0) break;
	}
	data = value;
	return inputStream;
}

// Instantiation of mc::var32's I/O methods.
template<> McIoOutputStream& 
mc::var32::write(McIoOutputStream& outputStream) const {
	return out<int32_t, 5, 15>(outputStream, *this);
}

template<> McIoInputStream& 
mc::var32::read(McIoInputStream& inputStream) {
	return in<int32_t, 5, 15>(inputStream, *this);
}

// Instantiation of mc::var64's I/O methods.
template<> McIoOutputStream& 
mc::var64::write(McIoOutputStream& outputStream) const {
	return out<int64_t, 10, 1>(outputStream, *this);
}

template<> McIoInputStream& 
mc::var64::read(McIoInputStream& inputStream) {
	return in<int64_t, 10, 1>(inputStream, *this);
}

inline bool McIoIsNotFollowerByte(char c) {
	return (c & 0xc0) != 0x80;
}

// The exception message for malformed string.
static const char* malformedString = "Malformed utf-8 string.";

// The utf-8 to utf-16 input converter.
template<typename Appender>
inline McIoInputStream& in(McIoInputStream& inputStream, 
		Appender& appender, size_t byteLength) {

	size_t i;
	for(i = 0; i < byteLength;) {
		int v = 0x080808080; 			// Filled with placeholder values.
		unsigned char* c = reinterpret_cast<unsigned char*>(&v);
		inputStream.read((char*)c, 1);	// Acquire leading byte.
		++ i;
		
		// Judge how to acquire and handle followed bytes.
		size_t followed = 0; 	// The number of bytes followed.
		int mask = 0;			// The mask applied to leading value.
		int offset = 0;			// The right-shifting offset.
		
		if(c[0] < 0x080) {
			followed = 0;
			mask = 0x7f;
			offset = 18;
		}
		else if(c[0] >= 0x0c0 && c[0] < 0x0e0) {
			followed = 1;
			mask = 0x1f;
			offset = 12;
		}
		else if(c[0] >= 0x0e0 && c[0] < 0x0f0) {
			followed = 2;
			mask = 0x0f;
			offset = 6;
		}
		else if(c[0] >= 0x0f0 && c[0] < 0x0f8) {
			followed = 3;
			mask = 0x07;
			offset = 0;
		}
		else throw std::runtime_error(malformedString);
		
		// Convert it to a single character.
		if(followed > 0) {
			inputStream.read((char*)&c[1], followed);
			i += followed;
		}
		if(	McIoIsNotFollowerByte(c[1]) || 
			McIoIsNotFollowerByte(c[2]) || 
			McIoIsNotFollowerByte(c[3]))
			throw std::runtime_error(malformedString);
		int characterValue =   (((c[0] & mask) << 18) |
		                        ((c[1] & 0x3f) << 12) |
		                        ((c[2] & 0x3f) <<  6) |
		                        ((c[3] & 0x3f) <<  0)) >> offset;
								
		// Convert character value to utf-16 string.
		if(characterValue < 0x010000) 
			appender.append((char16_t)characterValue);
		else {
			// Convert to surrogate pair.
			characterValue -= 0x010000;
			int highSurrogate = (characterValue >> 10) & 0x03ff;
			int lowSurrogate  = (characterValue >> 0)  & 0x03ff;
			appender.append((char16_t)(0xD800 | highSurrogate));
			appender.append((char16_t)(0xDC00 | lowSurrogate));
		}
	}
	if(i != byteLength) throw std::runtime_error(malformedString);
	return inputStream;
}

// Implementation for the read utf-16 string (length known) method.
McIoInputStream& McIoReadUtf16String(McIoInputStream& inputStream, 
		size_t byteLength, std::u16string& resultString) {
	struct McIoVectorAppender {
		// Stores code points in the string.
		std::vector<char16_t> stringBuilder;
		
		// Constructor for the builder, byteLength / 2 elements should be reserved.
		// when all characters are 1-byte utf-8, the size will be byteLength.
		// When all characters are 2-byte utf-8, the size will be byteLength / 2.
		// when all characters are 3-byte utf-8, the size will be byteLength / 3.
		// When all characters are 4-byte utf-8, the size will be byteLength.
		McIoVectorAppender(size_t byteLength): stringBuilder() {
			stringBuilder.reserve(byteLength / 2);
		}
		
		// Append new code unit to the string.
		void append(char16_t codeUnit) {
			stringBuilder.push_back(codeUnit);
		}
		
		// Retrieve the new string.
		std::u16string toString() {
			return std::u16string(stringBuilder.data(), stringBuilder.size());
		}
	};
	
	// Perform conversion.
	McIoVectorAppender appender(byteLength);
	McIoInputStream& resultStream = in<McIoVectorAppender>
				(inputStream, appender, byteLength);
	resultString = appender.toString();
	return resultStream;
}