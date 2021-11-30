#include "TaskScheduler.h"
#include "common/string/String.h"
#include "application/Config.h"
#include "Profiler.h"
#include "Log.h"
#include <ext/WindowsWrapper.h>
#include <processthreadsapi.h>
#include <math.h>

CVar CVar_ThreadLimit("Thread limit", ConfigType::Int,
	"Thread limit for background tasks. Must also check UseThreadLimit for this to be used. If not checked Nanoforge will use a default setting that leaves some CPU usage for your system.",
	ConfigValue(0),
	true  //ShowInSettings
);
CVar CVar_UseThreadLimit("Use thread limit", ConfigType::Bool,
	"If checked, uses Thread Limit. Mouse over thread limit to get more info on what it does.",
	ConfigValue(true), //Default value
	true  //ShowInSettings
);

u32 TaskScheduler::maxThreads_ = 0;
std::vector<std::thread> TaskScheduler::threads_ = { };
std::vector<Handle<Task>> TaskScheduler::threadTasks_ = { };
std::queue<QueuedTask> TaskScheduler::taskQueue_ = { };
std::condition_variable TaskScheduler::taskQueuePushCondition_;
std::mutex TaskScheduler::taskQueueMutex_;
bool TaskScheduler::stopAllThreads_ = false;

void TaskScheduler::Init()
{
	//Determine maximum amount of threads that can be used for tasks
	maxThreads_ = std::thread::hardware_concurrency();
	if (maxThreads_ <= 2)
		maxThreads_ = 1; //Require at least one thread for tasks
	else
		maxThreads_ -= 2; //Otherwise save 2 threads for the system

	//Get config vars
	{
		i32& threadLimit = CVar_ThreadLimit.Get<i32>();
		bool& useThreadLimit = CVar_UseThreadLimit.Get<bool>();

		//If threadLimit var == 0, fix it. Needs to be at least 1.
		if (threadLimit == 0)
		{
			threadLimit = maxThreads_;
			Config::Get()->Save();
		}

		//Override thread limit if UseThreadLimit is true
		if (useThreadLimit && (u32)threadLimit <= std::thread::hardware_concurrency())
			maxThreads_ = std::min((u32)threadLimit, std::thread::hardware_concurrency() - 1);
	}

	//Start thread pool threads
	threads_.reserve(maxThreads_);
	threadTasks_.reserve(maxThreads_);
	for (u32 i = 0; i < maxThreads_; i++)
	{
		threads_.push_back(std::thread(&TaskScheduler::ThreadWaitForTask, i));
		threadTasks_.push_back(nullptr);
	}

	Log->info("Task scheduler initialized. Started {} task threads out of {} hardware threads", maxThreads_, std::thread::hardware_concurrency());
}

void TaskScheduler::Shutdown()
{
	stopAllThreads_ = true;
	taskQueuePushCondition_.notify_all();
	for (auto& thread : threads_)
		thread.join();
}

void TaskScheduler::QueueTask(Handle<Task> task, std::function<void()> function)
{
	//Create new task and push onto queue
	{
		std::unique_lock<std::mutex> lock(taskQueueMutex_);
		task->completed_ = false;
		taskQueue_.push({ function, task });
	}

	//Notify waiting threads that a task was queued
	taskQueuePushCondition_.notify_one();
}

void TaskScheduler::ThreadWaitForTask(u32 threadIndex)
{
	string threadName = fmt::format("Task thread {}", threadIndex);
#ifndef WIN7_COMPATIBILITY_MODE //SetThreadDescription requires win10 v1607 or higher
	SetThreadDescription(threads_[threadIndex].native_handle(), String::ToWideString(threadName).c_str()); //Set name for debuggers
#endif
	PROFILER_SET_THREAD_NAME(threadName.c_str()); //Set name for profiler

	//Run tasks as they are queued up
	while (true)
	{
		QueuedTask task;
		Handle<Task> taskData = nullptr;
		{
			//Wait until a task is queued or the scheduler is stopped.
			std::unique_lock<std::mutex> lock(taskQueueMutex_);
			taskQueuePushCondition_.wait(lock, []() { return !taskQueue_.empty() || stopAllThreads_; });
			if (stopAllThreads_)
				return;

			//Pop new task from the queue
			task = taskQueue_.front();
			taskQueue_.pop();
			taskData = std::get<1>(task);

			//Skip task if it was cancelled
			if (taskData->Cancelled())
				continue;
		}

		//Run task
		threadTasks_[threadIndex] = taskData;
		taskData->running_ = true;
		std::get<0>(task)();

		//Signal that the task is complete
		taskData->completed_ = true;
		taskData->waitCondition_.notify_all();
		taskData->running_ = false;
		threadTasks_[threadIndex] = nullptr;
	}
}