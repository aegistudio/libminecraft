#pragma once
/**
 * @file libminecraft/multiplexer.hpp
 * @brief I/O Multiplexer Interface
 * @author Haoran Luo
 *
 * This file defines interface related to I/O multiplexer that runs in the main
 * loop. The multiplexer holds a settable timer (with default timeout 50 ms, 
 * which is a usual timer), and will perform I/O until the timeout has reached.
 *
 * The file descriptor managed by multiplexer should derive from the McIoDescriptor,
 * which encapsulates logic related to the descriptor, and manages resource for the
 * descriptor.
 *
 * @warning Neither multiplexer nor file descriptor is thread-safe, and there's no 
 * need for multi-threading in minecraft.
 */
#include <memory>

/**
 * @brief Defines event flags for I/O events that might be registered to and 
 * reported by the multiplexer.
 */
enum McIoEvent {
	/// Clear I/O event flags.
	evNone = 0,
	
	/// Register or report readable event, allowing later read() call.
	evIn = 1 << 0,
	
	/// Register or report writable event, allowing later write() call.
	evOut = 1 << 1,
};

/**
 * @brief Clear mask in the event flag, no matter that the event flag 
 * has set or not.
 */
inline McIoEvent McIoEventBitClear(McIoEvent value, McIoEvent bit) {
	return McIoEvent((value | bit) ^ bit);
}

/**
 * @brief Indicates the I/O event transition.
 * @see McIoNextStatus McIoDescriptor::handle(McIoEvent&)
 */
enum McIoNextStatus {
	/// Indicates to add the file descriptor back to the polling queue.
	nstPoll,

	/// Indicates this is just a yielding from the file descriptor.
	nstMore,
	
	/// Indicates no more I/O will be performed, and will be added to the 
	/// destruction queue.
	nstFinal
};

// For class name reference in descriptor.
class McIoMultiplexer;

/**
 * @brief The file descriptor object encapsulating related logic.
 */
class McIoDescriptor {
public:
	/**
	 * @brief The file descriptor managed and referred by this object.
	 *
	 * The object should close the handle when it get destructed. And when its
	 * value is -1, no closing would be performed. This is valid when performing
	 * move sematics.
	 */
	const int fd;

	/**
	 * @brief The default super-class constructor for the class.
	 *
	 * @param[in] fd The file descriptor should be constructed prior to the class.
	 * @param[in] initEventFlag The initial I/O event flags.
	 */
	McIoDescriptor(int fd, McIoEvent initEventFlag);
	 
	/// Copy sematics and move sematics are not allowed.
	McIoDescriptor(const McIoDescriptor&) = delete;
	McIoDescriptor& operator=(const McIoDescriptor&) = delete;
	McIoDescriptor(McIoDescriptor&&) = delete;
	McIoDescriptor& operator=(McIoDescriptor&&) = delete;
	
	/// The class should be pure virtual and so is destructor.
	virtual ~McIoDescriptor() noexcept;
	
	/// @brief Retrieve the multiplexer managing this file descriptor.
	McIoMultiplexer& getMultiplexer() const;
	
	/// @brief Retrieve the current event flags.
	McIoEvent currentEventFlag() const;
	
	/**
	 * @brief Change the I/O event flag.
	 *
	 * This method is NOT MT-Safe, which means the caller and the I/O multiplexer
	 * ought to be on the same thread, or some additional synchronization is 
	 * required (actually I don't see requirements for multithread manipulation on
	 * I/O for minecraft).
	 *
	 * @param newFlag the I/O flag to update.
	 */
	void updateEventFlag(McIoEvent newFlag);

	/// The descriptor control block size.
	static const size_t descriptorControlBlockSize = 64;
private:
	/// @brief The control field used by the I/O multiplexer.
	/// The control field that is platform dependent and whose details should 
	/// be hidden from the user.
	/// An I/O descriptor could only be added to one multiplexer, and managed
	/// by that multiplexer. (Adding a descriptor to other multiplexer will 
	/// cause exception thrown).
	char control[descriptorControlBlockSize];
	
	// The multiplexer needs direct access to the control field.
	friend class McIoMultiplexer;
	
	/**
	 * @brief Handles the I/O event and transit.
	 *
	 * Invoking this method indicates (currentEventFlag() & event != 0) and file 
	 * descriptor has been removed from the I/O multiplexer's notification queue.
	 * (act the same as EPOLLET | EPOLLONESHOT under linux). So one of the EAGAIN or 
	 * EWOULDBLOCK status ought to be achieved before adding the descriptor back.
	 *
	 * To add the a descriptor back to the polling queue after handling the event,
	 * return status with type == nstPoll. To claim a destruction on the descriptor,
	 * return status with type == nstFinal. And to yield processing slice piece, return
	 * status with type == nstMore.
	 *
	 * If an exception is thrown from this method without being captured, the descriptor
	 * will be regarded as returning nstFinal and removed from the multiplexer. The level
	 * of exception-safe guarantee is defined by underlying implementation.
	 *
	 * @param[inout] eventFlag the event that is happening, when the descriptor was
	 * just removed from the notification queue, the event flag is equal to the event
	 * that has just happened. However, if the implementation could update the event 
	 * flag, and the returned result is nstMore, the parameter while calling handle 
	 * method next time will be the updated value.
	 *
	 * @return the new status for the descriptor.
	 */
	virtual McIoNextStatus handle(McIoEvent& eventFlag) = 0;
};

/**
 * @brief The multiplexer that perform I/O multiplexing on the descriptors.
 */
class McIoMultiplexer {
public:
	/// The multiplexer's pimpl block size.
	static const size_t multiplexerControlBlockSize = 128;

	/// The default timeout value for the multiplexer, which is one default tick in minecraft.
	static const int defaultMinecraftTick = 5e7;
	
	// Constructor for the multiplexer.
	McIoMultiplexer();

	// Destructor for the multiplexer.
	~McIoMultiplexer() noexcept;
	
	// Copy sematics and move sematics are not allowed.
	McIoMultiplexer(const McIoMultiplexer&) = delete;
	McIoMultiplexer& operator=(const McIoMultiplexer&) = delete;
	McIoMultiplexer(McIoMultiplexer&&) = delete;
	McIoMultiplexer& operator=(McIoMultiplexer&&) = delete;
	
	/**
	 * @brief Make the multiplexer manages the constructed descriptor.
	 *
	 * It is likely that the descriptor cannot be added due to some system resource
	 * limitation settings, and exception will be thrown this case. Using unique pointer
	 * could gracefully manages the resource.
	 *
	 * @param descriptor the unique pointer of newly created descriptor. If the 
	 * managed descriptor could be added to the multiplexer, the pointer no longer
	 * manage the underlying object. Else an exception is thrown and the descriptor 
	 * continues to manage the object.
	 *
	 * @param descriptor the descriptor to insert, managed by an unique pointer.
	 */
	void insert(std::unique_ptr<McIoDescriptor>& descriptor);
	
	/**
	 * @brief Instantly remove and destroy the descriptor.
	 *
	 * This method is required as it is capable of removing infinitely sleeping
	 * descriptors, whose handle() method will never be called. However, for 
	 * normal descriptors, calling handle() method instead of remove() is preferred,
	 * though they will do the same.
	 *
	 * @param descriptor the descriptor to remove.
	 */
	void erase(McIoDescriptor* descriptor);
	
	/**
	 * @brief Retrieve the current timeout.
	 *
	 * Without being updated, the timeout for the multiplexer is 50ms, or
	 * 5e7 returned as nanoseconds.
	 *
	 * @return current timeout, in the unit of nanoseconds.
	 */
	unsigned long currentTimeout() const;
	
	/**
	 * @brief Set the timeout for the multiplexer
	 * @param[in] timeout the new timeout, in the unit of 
	 * nanoseconds.
	 */
	void updateTimeout(unsigned long timeout);
	
	/**
	 * @brief Run the multiplexer's polling loop, until the timeout is reached.
	 */
	void execute();
private:
	/// The pointer-to-impl of the multiplexer.
	char control[multiplexerControlBlockSize];
};