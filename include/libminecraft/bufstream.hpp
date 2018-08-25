#pragma once
/**
 * @file libminecraft/bufstream.hpp
 * @brief I/O Buffer Stream
 * @author Haoran Luo
 *
 * Defines concrete stream which are built on concrete buffers.
 */
#include "libminecraft/stream.hpp"

/**
 * @brief Defines buffer input stream which wraps an immutable 
 * buffer. The buffer is not managed by the stream.
 *
 * The stream performs boundary check, and exception will be 
 * thrown if boundary has exceeds.
 */
class McIoBufferInputStream : public McIoInputStream {
	/// The underlying buffer to read data from.
	const char* buffer;
	
	/// The size of the buffer.
	size_t size;
public:
	/// The constructor for the stream.
	McIoBufferInputStream(const char* initBuffer, size_t initSize):
		buffer(initBuffer), size(initSize) {}
	
	/// The implementation for the read method.
	virtual void read(char* buffer, size_t receiveLength) override;
};