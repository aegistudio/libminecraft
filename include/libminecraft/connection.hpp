#pragma once
/**
 * @file libminecraft/connection.hpp
 * @brief I/O Packet Socket Interface
 * @author Haoran Luo
 *
 * This connection interface isolates handling inbound packets
 * from the reading packet socket data from a non-blocking socket, 
 * regarding the pattern appeared in the data stream.
 */
#include "libminecraft/multiplexer.hpp"
#include "libminecraft/stream.hpp"
#include "libminecraft/writable.hpp"

class McIoConnection : public McIoDescriptor, public McIoWritable {
public:
	/// @brief Construct an instance of minecraft packet socket.
	McIoConnection(int sockfd);
	
	/// @brief Destruct the client socket instance.
	virtual ~McIoConnection() noexcept;
	
	// Restrict copy and move semantics.
	McIoConnection(const McIoConnection&) = delete;
	McIoConnection& operator=(const McIoConnection&) = delete;
	McIoConnection(McIoConnection&&) = delete;
	McIoConnection& operator=(McIoConnection&&) = delete;
	
	/**
	 * @brief Set the maximum packet size that is allowed, in unit of byte.
	 * The maximum packet size is initially set to 0, which means no packet
	 * size restriction is enabled.
	 *
	 * Setting a maximum packet size could help avoiding flooding attack, 
	 * which intends to cause the server to allocate a great chunk of data.
	 *
	 * @param[in] newSize the newly updated maximum packet size. When the 
	 * size is set to 0, it means no packet size checking is performed.
	 */
	void setMaximumPacketSize(size_t newSize);
	
	/// @brief Retrieve current packet size restriction.
	size_t getMaximumPacketSize() const;
	
	/// The control block size of the underlying data.
	static const size_t socketControlBlockSize = 128;
private:
	/// The pimpl-style control field.
	char control[socketControlBlockSize];
	
	/// @brief Implementation for the handling method.
	virtual McIoNextStatus handle(McIoEvent& events) override;
	
	/**
	 * @brief When the packet data is prepared, recast the prepared
	 * data to the concrete handling method.
	 *
	 * @param[inout] inputStream the input stream wrapping the 
	 * prepared data.
	 * @throw std::exception under any circumstance an exception is 
	 * thrown, the stream will become read-closed, and when there's
	 * no more data to send to a read-closed stream, the socket will
	 * be removed from the multiplexer.
	 */
	virtual void handleData(McIoInputStream& inputStream) = 0;
protected:
	/**
	 * @brief Indicate the client to be disconnected, when the data 
	 * transmission has completed, it will close automatically.
	 *
	 * When this method is called, no more data will be read and 
	 * handled. This method helps the server to gracefully shut
	 * down a connection.
	 */
	void indicateDisconnect() noexcept;
};