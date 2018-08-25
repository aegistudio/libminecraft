/**
 * @file multiplexer_linux.cpp
 * @brief Implementation for multiplexer.hpp under linux platform.
 * @author Haoran Luo
 *
 * For interface specification, please refer to the corresponding header. The multiplexing 
 * implementation is based on epoll and timerfd under linux.
 *
 * @see libminecraft/multiplexer.hpp
 */
 
#include <libminecraft/multiplexer.hpp>
#include <unordered_map>
#include <cassert>
#include <cstdint>

#include <unistd.h>
#include <fcntl.h>

#include <sys/epoll.h>
#include <sys/timerfd.h>

// The nanoseconds that one second contains.
static const unsigned long nanosecondUpperBound = 1e9;

// The lowerbound for nanoseconds that would not disable the timer.
static const unsigned long nanosecondLowerBound = 1e6;

// The descriptor control block under linux.
// Assigned responsibilities:
// - Trace whether the descriptor is associated with a multiplexer.
// - Trace the active events and listening events for handle().
// - Trace whether the object is inside the body of handle() or not.
// - Epoll-controling the descriptor on request and according to the 
//   object's life time.
// - Controlling the queue on request and according to the the object's
//   life time.
struct McIoDescriptorControl {
	/// The multiplexer managing this descriptor.
	McIoMultiplexer* multiplexer;
	int epollfd;
	
	/// The corresponding descriptor.
	McIoDescriptor* const descriptor;
	const int fd;
	
	/// The registered event flags for polling().
	McIoEvent listeningEvent;
	
	/// The remaining event flags for executing handle().
	McIoEvent activeEvent;
	
	/// Whether it is inside the descriptor's handle() block.
	/// Could affect the behavior of maintaining the block in container.
	bool executing;

	/// Whether it is marked to be destructed.
	/// Could affect the behavior of maintaining the block in container.
	bool markedRemoval;
	
	/// The linked list pointers, used to hold descriptors in different queues.
	McIoDescriptorControl **prevNext, *next;
	
	/// Move control block between queues.
	void moveQueue(McIoDescriptorControl** newQueue) noexcept {
		// Remove the node from the previous queue.
		if(prevNext != nullptr) {
			*prevNext = next;
			if(next != nullptr)
				next -> prevNext = prevNext;
		}
		
		// Reset the queue status.
		if(newQueue != nullptr) {
			prevNext = newQueue;
			next = *newQueue;
			*newQueue = this;
		}
		else {
			prevNext = nullptr;
			next = nullptr;
		}
	}
	
	/// Control the file descriptor's action with epollfd, regardless of 
	/// whether it is executing.
	template<int action> inline void controlPoll();
	
	/// Construct the control block.
	McIoDescriptorControl(McIoDescriptor* thiz, McIoEvent initialEvent): 
		multiplexer(nullptr), epollfd(-1), descriptor(thiz), fd(descriptor -> fd),
		listeningEvent(initialEvent), activeEvent(McIoEvent::evNone), 
		executing(false), markedRemoval(false),
		prevNext(nullptr), next(nullptr) {}
	
	/// Destruct the control block.
	~McIoDescriptorControl() noexcept {
		if(multiplexer != nullptr) try {
			moveQueue(nullptr);	// Remove from current queue.
			controlPoll<EPOLL_CTL_DEL>();
		} catch(const std::exception&) 
		{ /* Do nothing, just ignore. */ }
	}
	
	/// Associate the file descriptor with a multiplexer.
	/// @throw std::runtime_error when the descriptor cannot be registered.
	void associate(McIoMultiplexer* newMultiplexer, int newEpollfd) {
		assert(multiplexer == nullptr);                         // Not associated.
		assert(newMultiplexer != nullptr && newEpollfd != -1);  // Valid parameters.
		
		// Attempt to add to the list, and perform association.
		try {
			multiplexer = newMultiplexer;
			epollfd = newEpollfd;
			controlPoll<EPOLL_CTL_ADD>();
		}
		catch(const std::exception& ex) {
			multiplexer = nullptr;
			epollfd = -1;
			throw ex;
		}
	}
};
static_assert(sizeof(McIoDescriptorControl) < McIoDescriptor::descriptorControlBlockSize,
		"Insufficient space for the descriptor control block.");
		
// The multiplexer control block under linux.
struct McIoMultiplexerControl {
	/// The epoll's file descriptor.
	int epollfd;
	
	/// The timer's file descriptor.
	/// The userdata for timerfd is always NULL, which distinguishes it from other 
	/// descriptors.
	int timerfd;
	
	/// The managed file descriptors.
	std::unordered_map<int, std::unique_ptr<McIoDescriptor> > descriptors;
	
	/// The queue of descriptors that could call handle method.
	McIoDescriptorControl *activeQueue;
	
	// Create the multiplexer's control block.
	McIoMultiplexerControl(unsigned long initialTimeout): epollfd(-1), timerfd(-1),
			descriptors(), activeQueue(nullptr) {
		try {
			// Create the epoll descriptor.
			epollfd = epoll_create1(0);
			if(epollfd == -1) throw std::runtime_error("Cannot create epoll descriptor.");
			
			// Create the timer descriptor and initialize the timer.
			timerfd = timerfd_create(CLOCK_MONOTONIC, O_NONBLOCK);
			if(timerfd == -1) throw std::runtime_error("Cannot create timer descriptor.");
			updateTimeout(initialTimeout);
			
			// Place the timer descriptor into the epoll queue.
			controlTimer<EPOLL_CTL_ADD>();
		}
		catch(const std::exception& ex) {
			// Close essential file handlers.
			if(epollfd != -1) close(epollfd);
			if(timerfd != -1) close(timerfd);
			
			// Rethrow exception.
			throw ex;
		}
	}
	
	// Remove the block from the descriptors.
	inline void erase(McIoDescriptorControl* descriptor) {
		assert(descriptor != nullptr);
		assert(descriptors.count(descriptor -> fd) > 0);
		descriptors.erase(descriptor -> fd);
	}
	
	// Control the timerfd's status relative to the epoll fd.
	template<int action>
	inline void controlTimer() {
		struct epoll_event timerfdEvent;
		timerfdEvent.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
		timerfdEvent.data.ptr = nullptr;
		if(epoll_ctl(epollfd, action, timerfd, &timerfdEvent) < 0)
			throw std::runtime_error("Error while controlling timer descriptor.");
	}
	
	// Destruct the multiplexer control block.
	~McIoMultiplexerControl() noexcept {
		if(epollfd != -1) close(epollfd);
		if(timerfd != -1) close(timerfd);
	}
	
	// Update current timeout.
	void updateTimeout(unsigned long timeout) {
		assert(timeout > nanosecondLowerBound); // Timeout should be big enough to avoid disabling timer.
		itimerspec timerSpecification;
		
		// Calculate timeout interval.
		if(timeout < nanosecondUpperBound) {
			timerSpecification.it_interval.tv_sec = 0;
			timerSpecification.it_interval.tv_nsec = timeout;
		} else {
			timerSpecification.it_interval.tv_sec = timeout / nanosecondUpperBound;
			timerSpecification.it_interval.tv_nsec = timeout % nanosecondUpperBound;
		}
		
		// Retrieve the current timestamp and forward it by one interval.
		if(clock_gettime(CLOCK_MONOTONIC, &timerSpecification.it_value) < 0) 
			throw std::runtime_error("Cannot get current timestamp.");
		timerSpecification.it_value.tv_sec += timerSpecification.it_interval.tv_sec;
		timerSpecification.it_value.tv_nsec += timerSpecification.it_interval.tv_nsec;
		if(timerSpecification.it_value.tv_nsec > nanosecondUpperBound) {
			timerSpecification.it_value.tv_sec += 1;
			timerSpecification.it_value.tv_nsec -= nanosecondLowerBound;
		}
		
		// Update the timer by with new value.
		if(timerfd_settime(timerfd, TFD_TIMER_ABSTIME, &timerSpecification, NULL) < 0)
			throw std::runtime_error("Cannot update timer descriptor.");
	}
	
	// Retrieve current timeout.
	unsigned long currentTimeout() {
		itimerspec timerSpecification;
		if(timerfd_gettime(timerfd, &timerSpecification) < 0)
			throw std::runtime_error("Cannot get current timer specification.");
		if(timerSpecification.it_interval.tv_sec == 0)
			return timerSpecification.it_interval.tv_nsec;
		else return timerSpecification.it_interval.tv_sec * nanosecondUpperBound 
			+ timerSpecification.it_interval.tv_nsec;
	}
};
static_assert(sizeof(McIoMultiplexerControl) < McIoMultiplexer::multiplexerControlBlockSize,
		"Insufficient space for the multiplexer control block.");
		
// The implementation of McIoDescriptorControl::controlPoll.
template<int action>
inline void McIoDescriptorControl::controlPoll(){
	assert(epollfd != -1 && fd != -1);
	
	// Control the events.
	struct epoll_event event;
	event.events = EPOLLET | EPOLLONESHOT | 
		((listeningEvent & McIoEvent::evIn)?  EPOLLIN  : 0)|
		((listeningEvent & McIoEvent::evOut)? EPOLLOUT : 0);
	event.data.ptr = this;
	if(epoll_ctl(epollfd, action, fd, &event) < 0)
		throw std::runtime_error("Error while controlling descriptor.");
}

// The implementation of McIoMultiplexer::execute().
static const size_t numEventEpoll = 16;
void McIoMultiplexer::execute() {
	McIoMultiplexerControl& controlBlock = *((McIoMultiplexerControl*)control);
	struct epoll_event eventEpoll[numEventEpoll];
	
	// Run the loop, until the timer descriptor has become readable.
	bool epollRunning = true;
	while(epollRunning) {
		int numEvents = epoll_wait(controlBlock.epollfd, eventEpoll, numEventEpoll,
				(controlBlock.activeQueue != nullptr)? 0 : -1);
		if(numEvents < 0) throw std::runtime_error("Error while polling events.");
		
		// Interpret each events.
		for(size_t i = 0; i < numEvents; ++ i) {
			if(eventEpoll[i].data.ptr != nullptr) {
				// The file descriptor is a normal file descriptor.
				McIoDescriptorControl* descriptorControl = 
					((McIoDescriptorControl*)eventEpoll[i].data.ptr);
				if((eventEpoll[i].events & EPOLLERR) != 0) {
					// If there's error with the descriptor, just destroy it.
					controlBlock.erase(descriptorControl);
				}
				else {
					// Or interpert the occured events and move it to the active queue.
					descriptorControl -> activeEvent = (McIoEvent)(
						((eventEpoll[i].events & EPOLLIN)?  McIoEvent::evIn  : 0)|
						((eventEpoll[i].events & EPOLLOUT)? McIoEvent::evOut : 0));
					descriptorControl -> moveQueue(&controlBlock.activeQueue);
				}
			}
			else {
				// The file descriptor is a timer descriptor.
				if((eventEpoll[i].events & EPOLLERR) != 0) 
					throw std::runtime_error("The timer descriptor has error.");
				else {
					// Read until there's no more content in the timer descriptor.
					uint64_t expiration; int timerStatus;
					size_t expirationSize = sizeof(expiration);
					while((timerStatus = read(controlBlock.timerfd, 
							&expiration, expirationSize)) == expirationSize);
					
					// Reactive the descriptor and mark the polling loop would exit.					
					if(timerStatus == -1 && (errno == EWOULDBLOCK || errno == EAGAIN)) {
						epollRunning = false;
						controlBlock.controlTimer<EPOLL_CTL_MOD>();
					}
					else throw std::runtime_error("The timer descriptor has error.");
				}
			}
		}
		
		// Run each handled events.
		// As descriptors might be removed while executing handle(), we need to update
		// the pointer according to conditions.
		// This approach does not work under multithread circumstance, as the multiplexer
		// is defined NOT to be shared among threads, this approach is currently safe.
		for(McIoDescriptorControl* current = controlBlock.activeQueue; current != nullptr;) {
			// Run the handle() method first.
			current -> executing = true;
			McIoNextStatus nextStatus = McIoNextStatus::nstFinal;
			try {
				assert(current -> descriptor != nullptr && current -> fd != -1);
				nextStatus = current -> descriptor -> handle(current -> activeEvent);
			} catch(...) {
				nextStatus = McIoNextStatus::nstFinal;
			}
			current -> executing = false;
			if(current -> markedRemoval) nextStatus = McIoNextStatus::nstFinal;
			
			// Work on the object and see what to do next.
			McIoDescriptorControl* next = current -> next;
			switch(nextStatus) {
				case McIoNextStatus::nstFinal: {
					// Destruct the object.
					current -> moveQueue(nullptr);
					controlBlock.erase(current);
				} break;
				
				case McIoNextStatus::nstPoll: {
					// Return the object back to the polling queue.
					// If it cannot be returned to the polling queue, then remove it.
					current -> moveQueue(nullptr);
					try {
						current -> controlPoll<EPOLL_CTL_MOD>();
					} catch(...) {
						controlBlock.erase(current);
					}
				} break;
				
				default: break;
			}
			current = next;
		}
	}
}

// Implementation for McIoMultiplexer::insert().
void McIoMultiplexer::insert(std::unique_ptr<McIoDescriptor>& descriptor) {
	McIoMultiplexerControl* multiplexerControl = (McIoMultiplexerControl*)control;
	
	// Precondition checking.
	assert(descriptor.get() != nullptr);
	McIoDescriptorControl* descriptorControl = (McIoDescriptorControl*)(descriptor -> control);
	assert((descriptorControl -> multiplexer) == nullptr);
	
	// Attempt to associate and transfer ownership.
	descriptorControl -> associate(this, multiplexerControl -> epollfd);
	multiplexerControl -> descriptors[descriptor -> fd] = std::move(descriptor);
}

// Implementation for McIoMultiplexer::erase().
void McIoMultiplexer::erase(McIoDescriptor* descriptor) {
	McIoMultiplexerControl* multiplexerControl = (McIoMultiplexerControl*)control;
	
	// Precondition checking.
	assert(descriptor != nullptr);
	McIoDescriptorControl* descriptorControl = (McIoDescriptorControl*)(descriptor -> control);
	assert((descriptorControl -> multiplexer) == this);
	
	// Judge by condition, to either remove it directly, or remove it after handle().
	if(descriptorControl -> executing) descriptorControl -> markedRemoval = true;
	else multiplexerControl -> erase(descriptorControl);
}

// Implementation for McIoMultiplexer::currentTimeout().
unsigned long McIoMultiplexer::currentTimeout() const {
	return ((McIoMultiplexerControl*)control) -> currentTimeout();
}

// Implementation for McIoDescriptor::updateTimeout().
void McIoMultiplexer::updateTimeout(unsigned long newTimeout) {
	((McIoMultiplexerControl*)control) -> updateTimeout(newTimeout);
}

// Implementation for McIoMultiplexer::McIoMultiplexer().
McIoMultiplexer::McIoMultiplexer() {
	// Placement-new the object in the hidden field.
	new (control) McIoMultiplexerControl(McIoMultiplexer::defaultMinecraftTick);
}

// Implementation for McIoMultiplexer::~McIoMultiplexer().
McIoMultiplexer::~McIoMultiplexer() {
	// Placement-delete the object in the hidden field.
	((McIoMultiplexerControl*)control) -> ~McIoMultiplexerControl();
}

// Implementation for McIoDescriptor::getMultiplexer().
McIoMultiplexer& McIoDescriptor::getMultiplexer() const {
	McIoDescriptorControl* controlBlock = (McIoDescriptorControl*)control;
	assert((controlBlock -> multiplexer) != nullptr);
	return *(controlBlock -> multiplexer);
}

// Implementation for McIoDescriptor::currentEventFlag().
McIoEvent McIoDescriptor::currentEventFlag() const {
	McIoDescriptorControl* controlBlock = (McIoDescriptorControl*)control;
	assert((controlBlock -> multiplexer) != nullptr);
	return controlBlock -> listeningEvent;
}

// Implementation for McIoDescriptor::updateEventFlag().
void McIoDescriptor::updateEventFlag(McIoEvent newEventFlag) {
	McIoDescriptorControl* controlBlock = (McIoDescriptorControl*)control;
	assert((controlBlock -> multiplexer) != nullptr);
	
	if(controlBlock -> executing) 
		controlBlock -> listeningEvent = newEventFlag;
	else {
		McIoEvent oldEventFlag = controlBlock -> listeningEvent;
		try {
			controlBlock -> listeningEvent = newEventFlag;
			controlBlock -> controlPoll<EPOLL_CTL_MOD>();
		}
		catch(const std::exception& ex) {
			controlBlock -> listeningEvent = oldEventFlag;
			throw ex;
		}
	}
}

// Implementation for McIoDescriptor::McIoDescriptor().
McIoDescriptor::McIoDescriptor(int fd, McIoEvent initEventFlag): fd(fd) {
	assert(fd != -1 && initEventFlag != McIoEvent::evNone);
	new (control) McIoDescriptorControl(this, initEventFlag);
}

// Implementation for McIoDescriptor::~McIoDescriptor().
McIoDescriptor::~McIoDescriptor() {
	((McIoDescriptorControl*)control) -> ~McIoDescriptorControl();
	close(fd);
}