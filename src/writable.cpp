/**
 * @file writable.cpp
 * @brief Implementation for writable.hpp.
 * @author Haoran Luo
 *
 * For interface specification, please refer to the corresponding header.
 * @see libminecraft/writable.hpp
 */
#include "libminecraft/writable.hpp"
#include <sys/sendfile.h>
#include <deque>
#include <unistd.h>
#include <cassert>
#include <cstring>

/// Identify the type of each writable node.
enum McIoWritableNodeType {
	wrnodeWrite = 0,	///< Invokes write() on the underlying object.
	wrnodeSendfile64,	///< Invokes sendfile64() on the underlying object.
};

/// Stores the number of node of each type.
struct McIoWritableNode {
	McIoWritableNodeType type;	///< The current node type.
	size_t count;				///< The number of node of such type.
};

/// Data node for a outbound data with write() method.
struct McIoWritableWriteNode {
	/// The buffer managed by this write node.
	const std::shared_ptr<char> buffer;
	
	/// The offset and size of this write node.
	size_t offset, size;
	
	/// Forward template definition.
	static constexpr McIoWritableNodeType nodeType = wrnodeWrite;
	
	/// The constructor of the write() node.
	McIoWritableWriteNode(const std::shared_ptr<char>& buffer, 
		size_t offset, size_t size): 
		buffer(buffer), offset(offset), size(size) {}
	
	/// Move constructor of the write() node.
	McIoWritableWriteNode(McIoWritableWriteNode&& node) noexcept:
		buffer(std::move(node.buffer)), 
		offset(node.offset), size(node.size) {}
	
	/// Judge whether there's no more content to write out.
	/// If returning true, the node will always be removed.
	bool empty() const { return size == 0; }
	
	/// Perform write() on the specified file descriptor.
	int write(int fd) {
		int numWritten = ::write(fd, buffer.get() + offset, size);
		assert(numWritten < size);
		if(numWritten > 0) {
			offset += numWritten;
			size -= numWritten;
		}
		return numWritten;
	}
	
	/// Perform an attempt write() on the specified file descriptor.
	static int castWrite(int fd, size_t size, const char* buffer) {
		return ::write(fd, buffer, size);
	}
};

/// Data node for a outbound data with sendfile64() method.
struct McIoWritableSendfile64Node {
	/// The file descriptor to send file with.
	int sendfd;
	
	/// The offset and size monitored by this write node.
	ssize_t offset; size_t size;
	
	/// Forward template definition.
	static constexpr McIoWritableNodeType nodeType = wrnodeSendfile64;
	
	/// The constructor of sendfile64() node.
	McIoWritableSendfile64Node(int sendfd, ssize_t offset, size_t size):
		sendfd(sendfd), offset(offset), size(size) {}
		
	/// Perform sendfile64() on the specified file descriptor.
	int write(int fd) {
		int numWritten = ::sendfile64(fd, sendfd, &offset, size);
		assert(numWritten < size);
		if(numWritten > 0) size -= numWritten;	// Offset is updated by the call.
		return numWritten;
	}
	
	static int castWrite(int fd, size_t size, int sendfd, ssize_t offset) {
		return ::sendfile(fd, sendfd, &offset, size); 
	}
};

/// The underlying data of the McIoWritable.
struct McIoWritableControl {
	/// Retrieve node of certain type.
	template <typename N> std::deque<N>& queueOf() noexcept;
	
	/// Stores the data outbound (by write). Will only use when writing 
	/// operation on the descriptor should block.
	std::deque<McIoWritableWriteNode> writeQueue;
	
	/// Stores the data outbound (by sendfile64). Will only use when writing
	/// operation on the descriptor should block.
	std::deque<McIoWritableSendfile64Node> sendfile64Queue;
	
	/// Stores a queue of node of each type. It is guaranteed that if such 
	/// queue is empty, all other queues should be empty.
	std::deque<McIoWritableNode> queue;
	
	/// Reference to the decorated descriptor.
	McIoDescriptor* const decorated;

	/// Whether indicateClose() has been called.
	bool closeIndicated;
	
	/// The control block constructor.
	McIoWritableControl(McIoDescriptor* decorated): writeQueue(), queue(), 
		decorated(decorated), closeIndicated(false) {}
	
	/// Push a node to the back of a typed queue, and update certain amount.
	template <typename N> inline void push(N&& n) {
		queueOf<N>().push_back(std::move(n));
		if(queue.empty()) queue.push_back({N::nodeType, 1});
		else if(queue.back().type != N::nodeType) 
			queue.push_back({N::nodeType, 1});
		else ++ queue.back().count;
	}
	
	/// Perform a typed write on certain queue, and return whether it should break now.
	template <typename N> inline bool typedHandleWrite() {
		auto& currentQueue = queueOf<N>();
		N& front = currentQueue.front();
		int numWritten = front.write(decorated -> fd);
		
		// Judge and update by event flags.
		if(numWritten == 0 || numWritten < -1) 
			// The descriptor has closed or has undefined behavior.
			throw std::runtime_error("The descriptor has already "
				"closed or exhibited undefined behaviors.");
		else if(numWritten == -1) {
			// The descriptor should be blocked.
			if((errno == EWOULDBLOCK) || (errno == EAGAIN)) return true;
			// The descriptor has other fatal error.
			else throw std::runtime_error("The descriptor has "
				"some not-allowed error with it.");
		}
		else {
			// Data has been successfully written out.
			if(!front.empty()) return true;
			else {
				currentQueue.pop_front();
				-- queue.front().count;
				if(queue.front().count == 0) 
					queue.pop_front();
				return false;
			}
		}
	}
	
	/// The prototype for the write method.
	template <typename Cn, typename... Args>
	inline void prototypeWrite(const Cn& castNode, size_t size, Args&& ...args) {
		typedef typename Cn::castNodeType T;
		
		if(size == 0 || closeIndicated) return;
		if(queue.empty()) {
			int numWritten = T::castWrite(decorated -> fd, size, args...);
			if(numWritten == 0) return; // The stream has already closed.
			else if(numWritten != size) {
				if(numWritten == -1) {
					if((errno == EWOULDBLOCK) || (errno == EAGAIN)) 
						numWritten = 0;
					else return;        // Error while writing.
				}
				push<T>(castNode(numWritten));
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
		else push<T>(castNode(0));
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
				bool shouldBreak = false;
				switch(queue.front().type) {
					// The underlying type is write().
					case wrnodeWrite: {
						shouldBreak = typedHandleWrite<McIoWritableWriteNode>();
					} break;
					
					// The underlying type is sendfile64().
					case wrnodeSendfile64: {
						shouldBreak = typedHandleWrite<McIoWritableSendfile64Node>();
					} break;
					
					// Unknown or unimplemened type.
					default: assert(false);
				}
				if(shouldBreak) break;
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

/// Retrieve the write() queue.
template<> std::deque<McIoWritableWriteNode>& McIoWritableControl::
queueOf<McIoWritableWriteNode>() noexcept { return writeQueue; }

/// Retrieve the sendfile64() queue.
template<> std::deque<McIoWritableSendfile64Node>& McIoWritableControl::
queueOf<McIoWritableSendfile64Node>() noexcept { return sendfile64Queue; }
		
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
	typedef McIoWritableWriteNode castNodeType;
	const char* buffer;
	size_t size;
	McIoCastNodeBuffer(const char* buffer, size_t size):
		buffer(buffer), size(size) {}
	
	// The cast node method.
	McIoWritableWriteNode operator()(size_t numWritten) const {
		assert(numWritten < size);
		McIoWritableWriteNode node(std::shared_ptr<char>(
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
			prototypeWrite(castBuffer, length, buffer);
}

// Cast node for a shared pointer.
struct McIoCastNodeSharedPointer {
	typedef McIoWritableWriteNode castNodeType;
	const std::shared_ptr<char>& sharedPointer;
	size_t offset, size;
	McIoCastNodeSharedPointer(const std::shared_ptr<char>& sharedPointer,
		size_t offset, size_t size): sharedPointer(sharedPointer),
		offset(offset), size(size) {}
			
	// The cast node method.
	McIoWritableWriteNode operator()(size_t numWritten) const {
		assert(numWritten < size);
		return McIoWritableWriteNode(sharedPointer, 
			offset + numWritten, size - numWritten);
	}
};

// Implementation for McIoWritable::write() with shared pointer.
void McIoWritable::write(const std::shared_ptr<char>& sharedPointer, 
		size_t offset, size_t length) {
	
	McIoCastNodeSharedPointer castPointer(sharedPointer, offset, length);
	((McIoWritableControl*)control) -> prototypeWrite(
			castPointer, length, sharedPointer.get() + offset);
}

/// Cast node for the sendfile, nothing special.
struct McIoCastNodeSendfile64 {
	typedef McIoWritableSendfile64Node castNodeType;
	int sendfd; ssize_t offset; size_t size;
	
	McIoCastNodeSendfile64(int sendfd, ssize_t offset, size_t size):
		sendfd(sendfd), offset(offset), size(size) {}
	
	McIoWritableSendfile64Node operator()(size_t numWritten) const {
		assert(numWritten < size);
		return McIoWritableSendfile64Node(sendfd, 
			offset + numWritten, size - numWritten);
	}
};

// Implementation for McIoWritable::sendfile().
void McIoWritable::sendfile(int sendfd, ssize_t offset, size_t size) {
	
	McIoCastNodeSendfile64 castSendfile(sendfd, offset, size);
	((McIoWritableControl*)control) -> prototypeWrite(
			castSendfile, size, sendfd, offset);
}

// Implementation for McIoWritable::indicateClose().
void McIoWritable::indicateWriteClose() noexcept {
	((McIoWritableControl*)control) -> closeIndicated = true;
}

// Implementation for McIoWritable::handleWrite().
McIoNextStatus McIoWritable::handleWrite(McIoEvent& activeEvent) {
	return ((McIoWritableControl*)control) -> handleWrite(activeEvent);
}