/**
 * @file writable.cpp
 * @brief Implementation for writable.hpp.
 * @author Haoran Luo
 *
 * For interface specification, please refer to the corresponding header.
 * @see writable.hpp
 */
#include "libminecraft/writable.hpp"
#include <list>
#include <unistd.h>
#include <cassert>
#include <cstring>

/// Data node for a outbound data.
struct McIoWritableNode {
	const std::shared_ptr<char> buffer;
	size_t offset, size;
	
	McIoWritableNode(const std::shared_ptr<char>& buffer, 
		size_t offset, size_t size): 
		buffer(buffer), offset(offset), size(size) {}
		
	McIoWritableNode(McIoWritableNode&& node):
		buffer(std::move(node.buffer)), 
		offset(node.offset), size(node.size) {}
};

/// The underlying data of the McIoWritable.
struct McIoWritableControl {
	/// Stores the data outbound. Will only use when writing operation
	/// on the descriptor should block.
	std::list<McIoWritableNode> queue;
	
	/// Number of bytes that has been written to the buffer.
	size_t currentWritten;
	
	/// Reference to the decorated descriptor.
	McIoDescriptor* const decorated;

	/// Whether indicateClose() has been called.
	bool closeIndicated;
	
	/// The control block constructor.
	McIoWritableControl(McIoDescriptor* decorated):
		queue(), currentWritten(0), decorated(decorated), 
		closeIndicated(false) {}
	
	/// The prototype for the write method.
	template <typename Cn>
	inline void prototypeWrite(const char* buf, size_t size, const Cn& castNode) {
		if(size == 0 || closeIndicated) return;
		if(queue.empty()) {
			int numWritten = ::write(decorated -> fd, buf, size);
			if(numWritten == 0) return; // The stream has already closed.
			else if(numWritten != size) {
				if(numWritten == -1) {
					if((errno == EWOULDBLOCK) || (errno == EAGAIN)) 
						numWritten = 0;
					else return;        // Error while writing.
				}
				queue.push_back(castNode(numWritten));
				try {
					decorated -> updateEventFlag(McIoEvent(
						decorated -> currentEventFlag() | McIoEvent::evOut));
				} catch(const std::runtime_error&) {
					// Don't throw, however claer the queue, as the content
					// could never be sent.
					queue.clear();
				}
			}
		}
		else queue.push_back(castNode(0));
	}
	
	/// Implements the handleWrite method.
	McIoNextStatus handleWrite(McIoEvent& activeEvents) {
		if((activeEvents & McIoEvent::evOut) == 0) {
			// Currently the the writing is not active.
			if(closeIndicated && queue.empty()) 
				return McIoNextStatus::nstFinal;
			else return McIoNextStatus::nstPoll;
		}
		else {
			// If the code goes into this branch, the write event must have been 
			// enabled.
			assert((decorated -> currentEventFlag() & McIoEvent::evOut) != 0);
			
			// Attempt to write data out if the queue is not empty.
			while(!queue.empty()) {
				McIoWritableNode& front = queue.front();
				int numWritten = ::write(decorated -> fd, front.buffer.get() 
					+ front.offset + currentWritten, front.size - currentWritten);
				
				// Judge and update by event flags.
				if(numWritten == 0 || numWritten < -1) 
					// The descriptor has closed or has undefined behavior.
					throw std::runtime_error("The descriptor has already "
						"closed or exhibited undefined behaviors.");
				else if(numWritten == -1) {
					// The descriptor should be blocked.
					if((errno == EWOULDBLOCK) || (errno == EAGAIN)) break;
					// The descriptor has other fatal error.
					else throw std::runtime_error("The descriptor has "
						"some not-allowed error with it.");
				}
				else {
					// Data has been successfully written out.
					currentWritten += numWritten;
					if(currentWritten < front.size) break;
					else {
						queue.pop_front();
						currentWritten = 0;
					}
				}
			}
			
			// Currently write events are enabled.
			if(queue.empty()) {
				activeEvents = McIoEventBitClear(activeEvents, McIoEvent::evOut);
				decorated -> updateEventFlag(McIoEventBitClear
					(decorated -> currentEventFlag(), McIoEvent::evOut));
				if(closeIndicated) 
					return McIoNextStatus::nstFinal;
				else return McIoNextStatus::nstPoll;
			}
			else return McIoNextStatus::nstPoll;
		}
	}
};
static_assert(sizeof(McIoWritableControl) < McIoWritable::writableControlBlockSize,
		"Insufficient space for the writable control block.");
		
// Implementation for McIoWritable::McIoWritable().
McIoWritable::McIoWritable(McIoDescriptor* decorated) {
	new (control) McIoWritableControl(decorated);
}

// Implementation for McIoWritable::~McIoWritable().
McIoWritable::~McIoWritable() {
	((McIoWritableControl*)control) -> ~McIoWritableControl();
}

/// Cast node for just a simple buffer.
struct McIoCastNodeBuffer {
	const char* buffer;
	size_t size;
	McIoCastNodeBuffer(const char* buffer, size_t size):
		buffer(buffer), size(size) {}
	
	// The cast node method.
	McIoWritableNode operator()(size_t numWritten) const {
		assert(numWritten <= size);
		McIoWritableNode node(std::shared_ptr<char>(
			new char[size - numWritten], std::default_delete<char[]>()),
			0, size);
		memcpy(node.buffer.get(), buffer + numWritten, size - numWritten);
		return node;
	}
};

// Implementation for McIoWritable::write() with buffer.
void McIoWritable::write(const char* buffer, size_t length) {
	McIoCastNodeBuffer castBuffer(buffer, length);
	((McIoWritableControl*)control) -> 
		prototypeWrite<McIoCastNodeBuffer>(buffer, length, castBuffer);
}

// Cast node for a shared pointer.
struct McIoCastNodeSharedPointer {
	const std::shared_ptr<char>& sharedPointer;
	size_t offset, size;
	McIoCastNodeSharedPointer(const std::shared_ptr<char>& sharedPointer,
		size_t offset, size_t size): sharedPointer(sharedPointer),
		offset(offset), size(size) {}
			
	// The cast node method.
	McIoWritableNode operator()(size_t numWritten) const {
		assert(numWritten <= size);
		return McIoWritableNode(sharedPointer, 
			offset + numWritten, size - numWritten);
	}
};

// Implementation for McIoWritable::write() with shared pointer.
void McIoWritable::write(const std::shared_ptr<char>& sharedPointer, 
		size_t offset, size_t length) {
	
	McIoCastNodeSharedPointer castPointer(sharedPointer, offset, length);
	((McIoWritableControl*)control) -> prototypeWrite<McIoCastNodeSharedPointer>
			(sharedPointer.get() + offset, length, castPointer);
}

// Implementation for McIoWritable::indicateClose().
void McIoWritable::indicateWriteClose() noexcept {
	((McIoWritableControl*)control) -> closeIndicated = true;
}

// Implementation for McIoWritable::handleWrite().
McIoNextStatus McIoWritable::handleWrite(McIoEvent& activeEvent) {
	return ((McIoWritableControl*)control) -> handleWrite(activeEvent);
}