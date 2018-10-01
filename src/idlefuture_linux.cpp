/**
 * @file chat.cpp
 * @brief Implementation for idle future's executor.
 * @author Haoran Luo
 *
 * For interface specification, please refer to the corresponding header.
 * @see libminecraft/idlefuture.hpp
 */
#include "libminecraft/idlefuture.hpp"
#include <sys/eventfd.h>
#include <stdexcept>
#include <unistd.h>

// Define and ensure the size of the counter is valid.
typedef unsigned long long iffd_t;
static_assert(sizeof(iffd_t) == 8, "Invalid idle future descriptor size.");

// Os specific method for creating idle descriptor.
int McOsCreateIdleFutureDescriptor() /*mayThrow*/ {
	int iffd = eventfd(0, EFD_NONBLOCK);
	if(iffd < 0) throw std::runtime_error("Cannot create idle future descriptor.");
	return iffd;
}

// The implementation for McIoIdleFuture::McIoIdleFuture().
McIoIdleFuture::McIoIdleFuture(): McOsExecutorService(), 
	McIoDescriptor(McOsCreateIdleFutureDescriptor(), McIoEvent::evIn), taskQueue() {}

// The implementation for McIoIdleFuture::~McIoIdleFuture().
McIoIdleFuture::~McIoIdleFuture() {}

// The implementation for McIoIdleFuture::enqueue().
void McIoIdleFuture::enqueue(std::unique_ptr<McOsFutureTask>& task) {
	if(taskQueue.empty()) {
		iffd_t v = 1; if(write(fd, &v, 8) != 8) 
			throw std::runtime_error("Invalid future enqueuing state.");
	}
	taskQueue.push(std::move(task));
}

// The implementation for McIoIdleFuture::handle().
McIoNextStatus McIoIdleFuture::handle(McIoEvent& eventFlag) {
	if((eventFlag & McIoEvent::evIn) == McIoEvent::evIn) {
		// Perform execution on each enqueued tasks.
		for(size_t i = 0; i < numHandleExecute && !taskQueue.empty(); ++ i) {
			try {
				// Just attempt to advance the task.
				if(!taskQueue.front()->advance())
					taskQueue.pop();
			}
			catch(const std::runtime_error& ex) {
				// The exception will be preserved, and the front task will be 
				// therefore removed.
				taskQueue.pop();
			}
		}
		
		// Return depending on whether there's more tasks waiting to run.
		if(!taskQueue.empty()) return McIoNextStatus::nstMore;
		else {
			iffd_t v; if(read(fd, &v, 8) != 8) 
				throw std::runtime_error("Invalid future dequeuing state.");
			return McIoNextStatus::nstPoll;
		}
	}
	else return McIoNextStatus::nstPoll;
}