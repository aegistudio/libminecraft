#pragma once
/**
 * @file libminecraft/future.hpp
 * @brief Future Service Interface
 * @author Haoran Luo
 *
 * Defines tasks that could be executed asynchronously, or should 
 * be executed asynchronously, especially for complex structured 
 * objects (parse, destruct, or anything that is time consuming).
 *
 * The tasks are run in iterative flavour. Each iteration advances
 * the task. The service interface cares nothing about the task's 
 * running time, but the task loses its meaning if its runtime
 * is not properly controlled.
 */
#include <memory>

/**
 * @brief The interface for a future task. 
 * 
 * The task ought to properly implements its advance() interface,
 * will means such class acquire a time slice for executing its
 * internal task.
 */
struct McOsFutureTask {
	/// Virtual destructor for the future task.
	virtual ~McOsFutureTask() noexcept {};
	
	/// The task progressing interface, indicates the task has 
	/// obtain its time slice.
	///
	/// @return true if the task has not finished, or false if 
	/// the task is finished, which will cause the task to be 
	/// destroyed then.
	/// @throw will cause the time to be destroyed too.
	virtual bool advance() = 0;
};

/**
 * @brief The executor service for executing future tasks.
 *
 * The executor service dequeues a future task in proper condition
 * and execute it if possible.
 */
struct McOsExecutorService {
	/// Virtual destructor for executor service.
	virtual ~McOsExecutorService() noexcept {};
	
	/// Enqueue a task into the executor service. However how 
	/// will the task be done and when will the task be done 
	/// depends purely on underlying implementation.
	virtual void enqueue(std::unique_ptr<McOsFutureTask>& task) = 0;
};