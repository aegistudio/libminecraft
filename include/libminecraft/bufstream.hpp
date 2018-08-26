#pragma once
/**
 * @file libminecraft/bufstream.hpp
 * @brief I/O Buffer Stream
 * @author Haoran Luo
 *
 * Defines concrete stream which are built on concrete buffers.
 */
#include "libminecraft/stream.hpp"
#include <vector>
#include <tuple>

/**
 * @brief Defines buffer input stream which wraps an immutable 
 * buffer. The buffer is not managed by the stream.
 *
 * The stream offers boundary check, and exception will be 
 * thrown if boundary has exceeds.
 */
class McIoBufferInputStream : public McIoInputStream {
	/// The underlying buffer to read data from.
	const char* buffer;
	
	/// The remaining size of the buffer.
	size_t size;
public:
	/// The constructor of the input stream.
	McIoBufferInputStream(const char* initBuffer, size_t initSize):
		buffer(initBuffer), size(initSize) {}
	
	/// The implemented read method.
	virtual void read(char* buffer, size_t receiveLength) override;
};

/**
 * @brief Defines buffer output stream which wraps an vector
 * of bytes. The buffer is managed by the stream, and either the 
 * raw data or the data with variant integer length prefix could be 
 * retrieved.
 *
 * The data in the buffer should not exceeds the variant int length 
 * (if such data is packet, it won't be received by the client),
 * and exception would be thrown this case.
 */
class McIoBufferOutputStream : public McIoOutputStream {
	/// The buffer to store the written data.
	mutable std::vector<char> buffer;
public:
	/// The constructor of the output stream.
	McIoBufferOutputStream(): buffer(5) { }
	
	/// The implemented write method.
	virtual void write(const char* buffer, size_t sendLength) override;
	
	/// Retrieve the raw data, and the size will be the raw data size.
	virtual std::tuple<size_t, const char*> rawData() const 
		{ return std::make_tuple(buffer.size() - 5, buffer.data() + 5); }
	
	/// Retrieve the length-prefixed data, where the size returned will 
	/// be the length of that prefixed data.
	virtual std::tuple<size_t, const char*> lengthPrefixedData() const;
};