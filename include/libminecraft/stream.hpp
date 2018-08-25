#pragma once
/**
 * @file libminecraft/stream.hpp
 * @brief I/O Stream Abstraction
 * @author Haoran Luo
 *
 * This file abstracts stream for higher level of programming. The 
 * stream serves as interface for sending and receiving data.
 *
 * The constrains for streams are:
 * - Data read from the stream are removed from the stream permanently.
 * - Only when the data transmission is complete will the reading 
 *   or writing statement return.
 * - If an exception is thrown while reading or writing the stream,
 *   the stream status will become undefined therefore and the 
 *   program should close the stream since it always indicates 
 *   failure of reading or writing more data.
 */
#include <cstddef>
 
/// @brief Abstraction for the input stream.
class McIoInputStream {
public:
	virtual ~McIoInputStream() {}
	
	/**
	 * @brief the very interface for receiving data from stream.
	 *
	 * @param[out] buffer the buffer to receive data from the stream.
	 * The buffer size that is available for modifying should be
	 * from buffer to buffer + receiveLength - 1.
	 * @param[in] receiveLength the length of the data to receive.
	 */
	virtual void read(char* buffer, size_t receiveLength) = 0;
};

/// @brief Abstraction for the output stream.
class McIoOuputStream {
public:
	virtual ~McIoOuputStream() {}
	
	/**
	 * @brief the very interface for sending data to stream.
	 *
	 * @param[in] buffer the buffer to send data to the stream. The 
	 * data from buffer to buffer + receiveLength - 1 will be sent.
	 * @param[in] sendLength the length of the data to send.
	 */
	virtual void send(const char* buffer, size_t sendLength) = 0;
};