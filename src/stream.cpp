/**
 * @file writable.cpp
 * @brief Implementation for concrete streams: bufstream.hpp.
 * @author Haoran Luo
 *
 * For interface specification, please refer to the corresponding header.
 * @see bufstream.hpp
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