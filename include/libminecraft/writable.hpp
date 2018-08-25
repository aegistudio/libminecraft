#pragma once
/**
 * @file libminecraft/writable.hpp
 * @brief I/O Writable Interface
 * @author Haoran Luo
 *
 * This writable interface decorates descriptors that are capable
 * of being written to. This interface embeds a queue inside and 
 * when the write() operation cannot be completed instantly, the 
 * data to write will be stored in the queue.
 *
 * Thanks to the std::shared_ptr supporting customized destructor,
 * the user can broadcast a message and manage the buffer passed
 * in as they like.
 *
 * Please notice the writable class DOES NOT inherits from the 
 * descriptor's interface, to avoid DDoD multi-inheritance problem.
 */
#include "libminecraft/multiplexer.hpp"
#include <cstdint>

/**
 * @brief The writable interface that manages non-blocking write
 * to the file descriptor.
 *
 * This class is NOT multi-thread safe, and never share instances 
 * of this class among threads.
 */
class McIoWritable {
public:
	/**
	 * @brief Construct an instance of the writable instance.
	 *
	 * @param[in] decorated the decorated file descriptor, usually
	 * 'this' when the descriptor inherits this class.
	 */
	McIoWritable(McIoDescriptor* decorated);
	
	/**
	 * @brief Destructor for the writable instance.
	 * It is made pure virtual so that a class must inherit it 
	 * and acquire the interfaces defined in the class.
	 */
	virtual ~McIoWritable() = 0;
	
	// Disable copy sematics and move sematics.
	McIoWritable(const McIoWritable&) = delete;
	McIoWritable& operator=(const McIoWritable&) = delete;
	McIoWritable(McIoWritable&&) = delete;
	McIoWritable& operator=(McIoWritable&&) = delete;
	
	/**
	 * @brief Writing data described in the buffer.
	 *
	 * If the data could not be written in a single write() 
	 * call wrapped in this method, it will be enqueued into
	 * the internal sending queue.
	 *
	 * @param[in] buffer the data to send.
	 * @param[in] length the length of data.
	 */
	void write(const char* buffer, size_t length);
	
	/**
	 * @brief Writing data described in the shared pointer.
	 *
	 * The sematics are the same as the write with buffer variant,
	 * but allowing user to use their self-defined destructor.
	 *
	 * Please notice that the default method of deleting an char
	 * array with shared pointer is std::default_delete<char[]>().
	 *
	 * This method is NOT multi-thread safe.
	 *
	 * @param[in] sharedPointer the data to send.
	 * @param[in] offset of the data in the shared pointer.
	 * @param[in] length the length of the data.
	 */
	void write(const std::shared_ptr<char>& buffer, 
			size_t offset, size_t length);
	
	/// The control block size of the underlying data.
	static const size_t writableControlBlockSize = 50;
private:
	/// The pimpl-style control field.
	char control[writableControlBlockSize];
protected:
	/**
	 * @brief In the descriptor's handle method, perform writing
	 * when the descriptor has been prepared to write.
	 *
	 * When the activeEvent & McIoEvent::evOut is zero, the 
	 * activeEvent stays indifferent, and the result will be 
	 * McIoNextStatus::nstPoll.
	 *
	 * When data still can't be fully written out in this invocation
	 * to handleWrite(), the activeEvent stays indifferent and the 
	 * result will be McIoNextStatus::nstMore.
	 *
	 * When all data has been successfully written out, or 
	 * there's no more data to write, the activeEvent ^= McIoEvent::evOut 
	 * and the result will be McIoNextStatus::nstPoll when indicateClose()
	 * has not been called, or McIoNextStatus::nstFinal when indicateClose()
	 * has been called. 
	 *
	 * When there's error writing data out, the activeEvent ^= 
	 * McIoEvent::evOut, and result will be zero, the result will
	 * hence be McIoNextStatus::nstFinal.
	 *
	 * @param[inout] activeEvent the currently active event flags.
	 * @return the next status of the write part of descriptor, the
	 * final state of the descriptor depends on the collected state.
	 */
	McIoNextStatus handleWrite(McIoEvent& activeEvent);
	
	/**
	 * @brief Mark the writable buffer as going to close, and no more
	 * data will be accepted into the queue, however data in the queue
	 * may still be written out (unless there's error writing to the 
	 * file).
	 */
	void indicateWriteClose() noexcept;
};