#pragma once
/**
 * @file libminecraft/markable.hpp
 * @brief Markable Input Stream
 * @author Haoran Luo
 *
 * Defines input stream interfaces that supports marking operations.
 * The client of the code could mark an input stream and capture its
 * current state, which could be reset to when required.
 */

#include "libminecraft/stream.hpp"
#include <memory>

/**
 * @brief The mark interface that is bound to its an input stream.
 */
class McIoStreamMark {
public:
	// Virtual destructor for a pure virtual interface.
	virtual ~McIoStreamMark() {}
	
	/// @brief Reset the input stream to the state when invoking the 
	/// inputStream.mark().
	virtual void reset() = 0;
};

/**
 * @brief The markable stream interface.
 *
 * The markable stream is always input stream as output stream does 
 * not have requirements for marking.
 */
class McIoMarkableStream : public McIoInputStream {
public:
	// Virtual destructor for a pure virtual interface.
	virtual ~McIoMarkableStream() {}
	
	/// @brief Creates a mark at the current input stream state.
	virtual std::unique_ptr<McIoStreamMark> mark() = 0;
};