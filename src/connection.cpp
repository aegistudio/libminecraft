/**
 * @file connection.cpp
 * @brief Implementation for connection.hpp.
 * @author Haoran Luo
 *
 * For interface specification, please refer to the corresponding header.
 * @see connection.hpp
 */
#include "libminecraft/connection.hpp"
#include "libminecraft/bufstream.hpp"
#include <memory>
#include <vector>
#include <queue>
#include <cstring>
#include <cassert>
#include <unistd.h>

/// Indicates the status of the connection state machine.
enum McIoConnectionStatus {
	/// Indicating the state machine that it is reading the length field of 
	/// a packet. Please notice that cstPacketLength + 0 to 
	/// cstPacketLength + 4 are belong to this status.
	cstPacketLength = 0,
#define cstPacketLengthOf(i) (McIoConnectionStatus)(cstPacketLength + i)
	
	/// An unreachable state, indicating the packet length has overflowed.
	cstPacketLengthOverflow = 5,
	
	/// Indicating the state machine that it is reading the packet data field.
	cstPacketData = 6
};

/// The concrete control block inside the connection's pimpl-field.
struct McIoConnectionControl {
	/// Stores the descriptor.
	int fd;
	
	/// Stores the current status of the connection state machine.
	McIoConnectionStatus status;
	
	/// Stores the current parsing packet size.
	size_t packetSize;
	
	/// Stores the packet size restriction.
	size_t maxPacketSize;
	
	/// Stores the number of bytes already in the inbound buffer.
	size_t readSize;
	
	/// Stores the packet data inbound. Please notice that it may be not used 
	/// while parsing a packet that does not cause the descriptor to sleep.
	std::vector<char> inboundBuffer;
	
	/// Marks whether disconnection is indicated.
	bool disconnectIndicated;
	
	/// The constructor of the control block.
	McIoConnectionControl(int fd): fd(fd), status(cstPacketLengthOf(0)), 
			packetSize(0), maxPacketSize(0), readSize(0),
			inboundBuffer(), disconnectIndicated(false) {}
	
	/// Implements the handleRead method.
	template<typename HandleData>
	inline McIoNextStatus handleRead(McIoEvent& activeEvent, HandleData handleData) {
		if(disconnectIndicated) {
			// If disconnection has already indicated, always return final.
			activeEvent = McIoEventBitClear(activeEvent, McIoEvent::evIn);
			return McIoNextStatus::nstFinal;
		}
		else if((activeEvent & McIoEvent::evIn) == 0) return McIoNextStatus::nstPoll;
		else {
			int readStatus;
			// Perform reading. The code is written in Duff's device style.
			switch(status) {
				// Commom code for variant length handling block.
#define cstPacketLengthBlock(i)                                                     \
				case cstPacketLengthOf(i): {                                        \
					char thizByte;                                                  \
					readStatus = ::read(fd, &thizByte, 1);                          \
					if(readStatus != 1) goto HandleReadStatus;                      \
					else {                                                          \
						packetSize |= (((int)thizByte) & 0x07f) << (i * 7);         \
						if((((int)thizByte) & 0x080) == 0) goto EndOfPacketLength;  \
						else status = cstPacketLengthOf(i + 1);                     \
					}                                                               \
				};

				// The four Duff's device staus.
				cstPacketLengthBlock(0)
				cstPacketLengthBlock(1)
				cstPacketLengthBlock(2)
				cstPacketLengthBlock(3)
				cstPacketLengthBlock(4)
				
				// The length has overflowed.
				default: {
					activeEvent = McIoEventBitClear(activeEvent, McIoEvent::evIn);
					return McIoNextStatus::nstFinal;
				};
				
				// The packet length has been parsed, now check length status.
				EndOfPacketLength: {
					if((packetSize == 0) || (maxPacketSize > 0 && packetSize > maxPacketSize)) {
						status = cstPacketLengthOverflow;	// Make it an invalid status.
						activeEvent = McIoEventBitClear(activeEvent, McIoEvent::evIn);
						return McIoNextStatus::nstFinal;
					}
					status = cstPacketData;
				};
				
				case cstPacketData: {
					// Attempt to utilize the stack space first.
					// We assume that almost all operations could be done within
					// one stack buffer, thus avoid usage of vector.
					char stackInboundBuffer[BUFSIZ];
					char* targetBuffer = stackInboundBuffer;
					
					// If there's remained data in the inbound buffer.
					// Use that buffer instead.
					if(!inboundBuffer.empty())
						targetBuffer = inboundBuffer.data();
					
					// If the packet is too large, also use the inbound buffer.
					else if(packetSize > BUFSIZ) {
						inboundBuffer.resize(packetSize);
						targetBuffer = inboundBuffer.data();
					}
					
					// Attempt to read data from the file.
					size_t remainedSize = packetSize - readSize;
					readStatus = ::read(fd, &targetBuffer[readSize], remainedSize);
					if(readStatus <= 0 || readStatus > remainedSize) 
						goto HandleReadStatus;
					else readSize += (size_t)readStatus;
					
					// Depending on whether data has prepared, perform handling 
					// or copying.
					if(readSize == packetSize) {
						// Construct the stream and call the handle method.
						McIoBufferInputStream stream(targetBuffer, packetSize);
						handleData(packetSize, stream);
						
						// Reset fields and transit back to packet length.
						readSize = 0;
						packetSize = 0;
						status = cstPacketLengthOf(0);
						
						// Also clear the buffer.
						{
							std::vector<char> emptyBuffer;
							inboundBuffer.swap(emptyBuffer);
						};
						return McIoNextStatus::nstMore;
					}
					else {
						// Data must be stored in the vector for further use.
						if(targetBuffer == stackInboundBuffer) {
							inboundBuffer.resize(packetSize);
							memcpy(inboundBuffer.data(), stackInboundBuffer, readSize);
						}
						
						// Clear event flags and would poll.
						activeEvent = McIoEventBitClear(activeEvent, McIoEvent::evIn);
						return McIoNextStatus::nstPoll;
					}
				};
				
				// The read status is not the length byte, anaylze and handle it.
				HandleReadStatus: {
					if(readStatus == -1) {
						if(errno == EWOULDBLOCK || errno == EAGAIN) {
							activeEvent = McIoEventBitClear(activeEvent, McIoEvent::evIn);
							return McIoNextStatus::nstPoll;	
						} else throw std::runtime_error("The descriptor some "
							"not allowed error with it.");
					}
					else throw std::runtime_error("The descriptor has already "
							"closed or exhibited undefined behaviors.");
				};
			}
		}
	}
};
static_assert(sizeof(McIoConnectionControl) < McIoConnection::socketControlBlockSize, 
	"Insufficient space for the connection control block.");

// Implementation for the McIoConnection::McIoConnection().
McIoConnection::McIoConnection(int sockfd): 
	McIoDescriptor(sockfd, McIoEvent::evIn), McIoWritable(this) {
	new (control) McIoConnectionControl(sockfd);
}

// Implementation for the McIoConnection::~McIoConnection().
McIoConnection::~McIoConnection() {
	((McIoConnectionControl*)control) -> ~McIoConnectionControl();
}

// Implementation for the McIoConnection::setMaximumPacketSize().
void McIoConnection::setMaximumPacketSize(size_t newSize) {
	((McIoConnectionControl*)control) -> maxPacketSize = newSize;
}

// Implementation for the McIoConnection::getMaximumPacketSize().
size_t McIoConnection::getMaximumPacketSize() const {
	return ((McIoConnectionControl*)control) -> maxPacketSize;
}

// Implementation for the McIoConnection::indicateDisconnect().
void McIoConnection::indicateDisconnect() noexcept {
	indicateWriteClose();
	((McIoConnectionControl*)control) -> disconnectIndicated = true;
}

// Implementation for the McIoConnection::handle().
McIoNextStatus McIoConnection::handle(McIoEvent& events) {
	// Attempt to perform I/O first.
	McIoNextStatus readNext = ((McIoConnectionControl*)control) -> handleRead(
		events, [this](size_t packetSize, McIoInputStream& inputStream) 
				{ handle(packetSize, inputStream); });
	McIoNextStatus writeNext = handleWrite(events);
	
	// Generalized by the status. (INVAL indicates impossible combination)
	// R \ W   More    Final   Poll
	// More    INVAL   INVAL   More
	// Final   INVAL   Final   Poll
	// Poll    INVAL   Final   Poll
	assert(writeNext != McIoNextStatus::nstMore);
	if(readNext == nstMore) {
		assert(writeNext != McIoNextStatus::nstFinal);
		return McIoNextStatus::nstMore;
	}
	else return writeNext;
}