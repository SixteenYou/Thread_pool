#include"thread_pool.h"

void wty::thread_pool::work_thread(std::shared_ptr<work_info> in)
{
	while (in->run)
	{
#ifndef concurrentqueue
		std::unique_lock<std::mutex>lock(*(in->task_mutex));
		in->thread_pool_cv->wait(lock, [&] {return !in->task_queue->empty() || !in->run;});
		if(in->run&&!in->task_queue->empty())//检查一下当前的线程是否还具备可运行的条件，不具备的话就直接退出当前执行的线程
		{
			auto it=in->task_queue->front();
			in->task_queue->pop();
			lock.unlock();
			it();
			continue;
		}
#else
		std::function<void()> task;
		if (in->task_queue->try_dequeue(task))
		{
			task();
			continue;
		}
		else
			std::this_thread::sleep_for(std::chrono::milliseconds(0));
#endif
		if (!in->run)
			break;
	}
}

wty::thread_pool::thread_pool()
{
	initialise_thread_pool();
}

wty::thread_pool::thread_pool(unsigned short min_t, unsigned short max_t)
{
	if (min_t <= 0 || max_t < min_t)
	{
		std::cout << "The minimum value cannot be less than or equal to 0, or the maximum value cannot be less than the minimum value." << std::endl;
		exit(1);
	}
	set_max_thread(min_t < max_t ? max_t : min_t);
	set_min_thread(min_t > max_t ? max_t : min_t);
	initialise_thread_pool();
}

wty::thread_pool::~thread_pool()
{
	run = false;
	examine_thread.join();
	std::unique_lock<std::mutex>lock(v_thread_mutex);
	for (auto& i : v_thread)
		i->run = false;
#ifndef concurrentqueue
	thread_pool_cv.notify_all();
#endif
	for (auto& i : v_thread)
		i->th.join();
}

bool wty::thread_pool::add_task(std::function<void()> f)
{
	if (run != true)
		return false;
#ifndef concurrentqueue
	std::unique_lock<std::mutex>lock(task_mutex);
	task_queue.push(std::move(f));
	lock.unlock();
	thread_pool_cv.notify_one();
#else
	task_queue.enqueue(std::move(f));
#endif
	return true;
}

void wty::thread_pool::initialise_thread_pool()
{
	run = true;
	std::unique_lock<std::mutex>lock(v_thread_mutex);
	while (v_thread.size() < min_thread)v_thread.push_back(std::make_shared<work_info>());
	for (auto& i : v_thread)
	{
#ifndef concurrentqueue
		i->task_mutex = &task_mutex;
		i->thread_pool_cv = &thread_pool_cv;
#endif
		i->task_queue = &task_queue;
		i->th = std::thread(work_thread, i);
	}
	examine_thread = std::thread(examine_thread_load, this);
}

void wty::thread_pool::set_max_thread(unsigned short size)
{
	if (size < min_thread)//最大数量不得低于最少数量
		return;
	max_thread = size;
}

void wty::thread_pool::set_min_thread(unsigned short size)
{
	if (size <= 0)
	{
		std::cout << "The minimum number of threads in the pool must not be less than or equal to zero" << std::endl;
		exit(1);
	}
	if (size > max_thread)//最小数量不得超过最大数量
		return;
	min_thread = size;
}

void wty::thread_pool::set_min_max_thread(unsigned short min_size, unsigned short max_size)
{
	if (min_size <= max_size)
	{
		set_min_thread(min_size);
		set_max_thread(max_size);
	}
}

void wty::thread_pool::set_waiting_time(size_t time)
{
	if (time <= max_waiting_time && time > 0)
		waiting_time = time;
}

void wty::thread_pool::examine_thread_load(thread_pool*info)
{
	bool full = false;
	while (info->run)
	{
		std::unique_lock<std::mutex>lock_v(info->v_thread_mutex);
		size_t now_task_max = now_max_tasks(info->v_thread.size());//当前最大能负载的任务量
		size_t now_task_min = now_min_tasks(info->v_thread.size());//当前最小缩容的最小负载量
		size_t max_thread_task = max_task(info->max_thread);//当前最大线程的任务量
		std::unique_lock<std::mutex>lock(info->task_mutex);
#ifndef concurrentqueue
		size_t now_task_count = info->task_queue.size();
#else
		size_t now_task_count = info->task_queue.size_approx();
#endif
		lock.unlock();
		if (now_task_count >= max_thread_task)//任务太多，如果没有我自己的日志系统，就打印到控制台
		{
			//优化一下，直接满增量扩展线程，避免一次就扩展那么一点线程
			full = true;
#ifdef WTY_LOG
			if(wty::log_obj->task_queue(wty::INFO::DEBUG ,"The current number of tasks in the thread pool is too high, and the management staff should take action."))
				std::cout << "Log_system error" << std::endl;
#else 
			std::cout << "DEBUG:The current number of tasks in the thread pool is too high, and the management staff should take action." << std::endl;
#endif
		}
		if (now_task_max < now_task_count)//如果当前任务量，超过了当前线程数的最大阈值，就出发扩容机制
		{
			if (info->max_thread > info->v_thread.size())//当前线程的数量，没有达到当前上限时，触发扩容，否则可用线程满了，无法触发扩容机制
			{
				size_t max = 0;
				if (info->v_thread.size() * 2 > info->max_thread)//如果没法做到扩容两倍，则直接扩容满即可，否则就扩容两倍
					max = info->max_thread;
				else
					max = info->v_thread.size() * 2;
				if(full)
				{
					max = info->max_thread;
					full = false;
				}
				while (info->v_thread.size() < max)info->v_thread.push_back(std::make_shared<work_info>());
				for (auto& i : info->v_thread)
				{
					if(i->th.get_id()== std::thread::id{})
					{
#ifndef concurrentqueue
						i->task_mutex = &info->task_mutex;
						i->thread_pool_cv = &info->thread_pool_cv;
#endif
						i->task_queue = &info->task_queue;
						i->th = std::thread(work_thread, i);
					}
				}
			}
		}
		else if (now_task_min > now_task_count)//当前任务量远低于当前可用线程的负载，尝试关掉多余线程
		{
			if (info->min_thread < info->v_thread.size())
			{
				size_t max = 0;
				if (info->v_thread.size() / 2 >= info->max_thread)//如果缩两倍还有余，那就缩两倍，否则就直接缩到最小的初始数量
					max = info->v_thread.size() / 2;
				else
					max = info->min_thread;
				for (size_t i = max;i < info->v_thread.size();i++)
					info->v_thread[i]->run = false;
#ifndef concurrentqueue
				info->thread_pool_cv.notify_all();
#endif
				for (size_t i = max;i < info->v_thread.size();i++)
					info->v_thread[i]->th.join();
				while (info->v_thread.size() > max)
					info->v_thread.pop_back();
			}
		}
		lock_v.unlock();//必须全程持锁，因为这个这个整体操作必须是原子操作，要不然万一出现析构的时候，析构掉数据，那就直接死了，休眠的时候在放锁
		std::this_thread::sleep_for(std::chrono::milliseconds(info->waiting_time));//c++的休眠函数，单位是毫秒
	}
}