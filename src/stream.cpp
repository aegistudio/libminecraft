/**
 * @file writable.cpp
 * @brief Implementation for concrete streams: bufstream.hpp.
 * @author Haoran Luo
 *
 * For interface specification, please refer to the corresponding header.
 * @see libminecraft/bufstream.hpp
 */
#include "libminecraft/stream.hpp"
#include "libminecraft/bufstream.hpp"
#include <stdexcept>
#include <cstring>

/// Implementation for the McIoBufferInputStream::read().
void McIoBufferInputStream::read(char* outBuffer, size_t receiveLength) {
	/// Make sure that enough data can be read from the buffer.
	if(receiveLength == 0) return;
	if(receiveLength > size) throw std::runtime_error(
			"Requested data has exceeded the available data.");
	
	// Perform reading and advance status.
	memcpy(outBuffer, buffer, receiveLength);
	buffer += receiveLength;
	size -= receiveLength;
}

/// Implementation for the McIoBufferInputStream::skip().
void McIoBufferInputStream::skip(size_t skipLength) {
	/// Make sure that enough data can be skipped from the buffer.
	if(skipLength == 0) return;
	if(skipLength > size) throw std::runtime_error(
			"Requested data has exceeded the available data.");
	
	// Perform skipping and advance status.
	buffer += skipLength;
	size -= skipLength;
}

/// The max allowed size of variant integer, which is 32-bit integer with most 
/// significant bit set to 0.
unsigned long maxVarintValue = 0x07ffffffful;

/// Implementation for the McIoBufferOutputStream::write().
void McIoBufferOutputStream::write(const char* inBuffer, size_t sendLength) {
	if(sendLength == 0) return;
	
	// Allocate more space in the buffer and perform writing.
	size_t originSize = buffer.size();
	size_t newSize = originSize + sendLength;
	if((unsigned long)newSize > maxVarintValue) 
		throw std::runtime_error("The data to send is too large.");
	buffer.resize(originSize + sendLength);
	memcpy(buffer.data() + originSize, inBuffer, sendLength);
}

/// Implementation for the McIoBufferOutputStream::lengthPrefixedData().
std::tuple<size_t, const char*> McIoBufferOutputStream::lengthPrefixedData() const {
	const size_t length = buffer.size() - 5;
	size_t byteLength = length;

	// Retrieve the least significant byte, convert it to the reserved buffer
	// space before data, then judge whether to convert more.
#define McIoBufferOutputLength(i)\
	buffer[i] = (char)(byteLength & 0x07f);\
	if(byteLength < 0x80) return std::make_tuple(length + 5 - i, buffer.data() + i);\
	buffer[i] = (char)(buffer[i] | 0x080);\
	byteLength = byteLength >> 7;
	
	// Concretely unroll the loop.
	McIoBufferOutputLength(4)
	McIoBufferOutputLength(3)
	McIoBufferOutputLength(2)
	McIoBufferOutputLength(1)
	
	// The least byte, omitting the condition branch.
	buffer[0] = (char)(byteLength & 0x07f);
	return std::make_tuple(length + 5, buffer.data());
}