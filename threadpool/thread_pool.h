#pragma once
#include<thread>
#include<iostream>
#include<condition_variable>
#include<vector>
#include<mutex>
#include <functional> 
#include<queue>
#include "../third_party/concurrentqueue.h"
namespace wty//我自己的命命空间
{
#define max_waiting_time (60*5*1000)
#define min_waiting_time (10*1000)
#define now_max_tasks(x) (x*2*10)
#define now_min_tasks(x) (x*2*5)
#define max_task(x) (x*2*100)
	class thread_pool
	{
	private:
		uint16_t min_thread = 5;//初始默认最低线程数
		uint16_t max_thread = 40;//初始默认最大线程数
		std::mutex task_mutex;//任务互斥锁
		std::condition_variable thread_pool_cv;//提醒某一个或者多个线程开始工作
#ifndef concurrentqueue
		std::queue<std::function<void()>>task_queue;//任务函数和任务数据队列
#else
		moodycamel::ConcurrentQueue<std::function<void()>>task_queue;//任务函数和任务数据队列
#endif
		std::atomic<bool>run = true;//线程池是否还可以再次增加任务
		std::thread examine_thread;//轮询检查线程
		struct work_info
		{
#ifndef concurrentqueue
			std::condition_variable* thread_pool_cv;
			std::mutex* task_mutex;
			std::queue<std::function<void()>>*task_queue;
#else
			moodycamel::ConcurrentQueue<std::function<void()>>*task_queue;
#endif
			std::atomic<bool>run = true;
			std::thread th;
		};
		size_t waiting_time= 100;//轮询检查的时间间隔，单位是毫秒，初始为10秒即10000毫秒，最大不得超过五分钟即60*5*1000
		std::mutex v_thread_mutex;
		std::vector<std::shared_ptr<work_info>> v_thread;//线程本体
		static void work_thread(std::shared_ptr<work_info> in);//具体的工作线程
		void initialise_thread_pool();//初始操作
		static void examine_thread_load(thread_pool*info);//每隔固定数量的时间检验一下任务的复杂度，这里主要是看任务量是否根据公式远超当前线程数量
	public:
		thread_pool();
		thread_pool(unsigned short min_t, unsigned short max_t=40);
		~thread_pool();
		void set_max_thread(unsigned short size);//设置最大线程数
		void set_min_thread(unsigned short size );//设置最小线程数
		void set_min_max_thread(unsigned short min_size, unsigned short max_size);//同时设置最大最小线程
		void set_waiting_time(size_t time);//设置轮询检查的时间间隔，单位是毫秒
		bool add_task(std::function<void()>);//添加任务
	};
}