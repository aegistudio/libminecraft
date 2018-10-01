#pragma once
/**
 * @file libminecraft/idlefuture.hpp
 * @brief Idle Future Service
 * @author Haoran Luo
 *
 * The future service that executes future tasks inside
 * the multiplexer, along with other multiplexed polling
 * descriptors. Such executor utilize the idle time of 
 * the io polling loops to digest bulky tasks.
 */

#include "libminecraft/future.hpp"
#include "libminecraft/multiplexer.hpp"
#include <queue>

/**
 * @brief Defines the idle future executor service.
 *
 * This interface is not designed to run under multithread condition, 
 * do not invoke the class from multiple threads.
 */
class McIoIdleFuture : public McOsExecutorService, public McIoDescriptor {
	std::queue<std::unique_ptr<McOsFutureTask>> taskQueue;
public:
	McIoIdleFuture();
	~McIoIdleFuture() noexcept;
	
	// The inherited idle interface.
	virtual void enqueue(std::unique_ptr<McOsFutureTask>& task) override;
	
	// Handle the event polling when it becomes available.
	virtual McIoNextStatus handle(McIoEvent& eventFlag) override;
	
	// How many tasks's advance() method will be called each handle cycle.
	// The number is set to avoid too much system call to the multiplexing method.
	static const size_t numHandleExecute = 16;
};