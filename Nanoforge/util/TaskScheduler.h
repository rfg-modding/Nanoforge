#pragma once
#include "common/Typedefs.h"
#include "Log.h"
#include <condition_variable>
#include <functional>
#include <thread>
#include <future>
#include <vector>
#include <mutex>
#include <queue>

class Task;
class Config;
class StatusBar;

/*std::function<void()> is stored outside of Task so any arguments can be bound to the Task with std::bind upon calling TaskScheduler::QueueTask().
  This is also so the Task instance can be created without access to the function and arguments being bound. This is convenient in situations
  where the arguments passed to the Task aren't available until after its construction.*/
using QueuedTask = std::tuple<std::function<void()>, Handle<Task>>;

//Queues tasks and runs them when a thread is available
class TaskScheduler
{
	friend StatusBar;
public:
	//Start task threads
	static void Init(Config* config);
	//Stop all threads and wait for them to exit
	static void Shutdown();
	//Queue a task to be run when a thread is available
	static void QueueTask(Handle<Task> task, std::function<void()> function);

private:
	//Primary function of task threads. Waits for tasks to be queued and runs them
	static void ThreadWaitForTask(u32 threadIndex);

	static u32 maxThreads_;
	static std::vector<std::thread> threads_;
	static std::vector<Handle<Task>> threadTasks_; //Tracks what task, if any that each thread is running
	static std::queue<QueuedTask> taskQueue_;
	static std::condition_variable taskQueuePushCondition_;
	static std::mutex taskQueueMutex_;
	static bool stopAllThreads_;
};

//Code that's run asynchronously
class Task
{
	friend TaskScheduler;

	//Private constructor so you can only construct tasks with Task::Create() as a Handle<Task>
	Task(const string& name) : Name(name)
	{

	}

public:
	static Handle<Task> Create(const string& name)
	{
		//Workaround to call the private constructor of Task since C++ doesn't let you do that directly
		struct TaskCreateHandleEnabler : public Task
		{
			TaskCreateHandleEnabler(const string& name) : Task(name) {}
		};
		return CreateHandle<TaskCreateHandleEnabler>(name);
	}

	//Returns true if the task ran and was completed
	bool Completed()
	{
		return completed_;
	}

	//Returns true if the task is running
	bool Running()
	{
		return running_;
	}

	//Cancels the task if it has started. Stops it if it is running
	void Cancel()
	{
		cancelled_ = true;
	}

	//Returns true if Stop() was called
	bool Cancelled()
	{
		return cancelled_;
	}

	//Waits for the task to complete, stop running, or for the provided wait time to elapse
	void Wait(u32 waitTimeMs = 0)
	{
		std::unique_lock lock(waitMutex_);

		//Wait until task is completed or the provided wait time elapses
		auto waitFunc = [this]() -> bool { return completed_ || !running_; };
		if (waitTimeMs == 0)
			waitCondition_.wait(lock, waitFunc);
		else
			waitCondition_.wait_for(lock, std::chrono::milliseconds(waitTimeMs), waitFunc);
	}

	//Cancel task and wait for it to stop running
	void CancelAndWait()
	{
		Cancel();
		Wait();
	}

	string Name;

private:
	std::mutex waitMutex_;
	std::condition_variable waitCondition_;
	bool cancelled_ = false;
	bool running_ = false;
	std::atomic<bool> completed_ = false;
};