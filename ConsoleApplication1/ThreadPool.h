#pragma once
#include <vector>
#include <queue>
#include <atomic>
#include <future>
#include <thread>
#include <functional>
#include <mutex>
#include <condition_variable>

using namespace std;
class ThreadPool
{
public:
	// 构造函数，创建线程池
	ThreadPool(size_t threads) : stop(false) {
		for (size_t i = 0; i < threads; ++i)
			workers.emplace_back(
				[this]
				{
					for (;;)//线程无线循环等待任务
					{
						std::function<void()> task;//声明一个返回值为void的函数对象
						{
							//创建互斥锁防止多个线程同时修改任务队列
							std::unique_lock<std::mutex> lock(this->queue_mutex);
							//当stop为true或任务队列中有任务的时候唤醒线程
							//在没有任务执行的时候不要占用cpu资源
							//调用条件变量使得程序暂停
							this->condition.wait(lock,
								[this] { return this->stop || !this->tasks.empty(); });
							//如果stop变量为true且任务队列为空，那么这个函数就返回，线程就结束。
							if (this->stop && this->tasks.empty())  return;
							//从任务队列中取出一个任务。std::move()函数用于将任务从队列中移出（以避免复制任务对象），
							//然后pop()函数移除队列中的这个任务。
							task = std::move(this->tasks.front());
							this->tasks.pop();
						}
						task();
					}
				}
				);
	}
	// 添加任务到线程池

	template<class F, class... Args>
	auto enqueue(F&& f, Args&&... args)//尾返回类型
		-> std::future<typename std::result_of<F(Args...)>::type>
	{
		//用于推断任务函数的返回结果
		using return_type = typename std::result_of<F(Args...)>::type;
		//，使用 std::forward<F>(f) 可以实现完美转发，确保 f 在被 std::bind 调用时保持其原始的值类别。
		//这样既可以保证性能，又可以提高函数的通用性，使函数可以接受任意类型的参数。
		//其模板参数 return_type() 表示存储在 packaged_task 中的函数的返回类型。
		auto task = std::make_shared< std::packaged_task<return_type()> >(
			std::bind(std::forward<F>(f), std::forward<Args>(args)...)
			);

		std::future<return_type> res = task->get_future();
		{
			std::unique_lock<std::mutex> lock(queue_mutex);//RAII的实现

			// 线程池停止时，禁止添加新的任务
			if (stop)
				throw std::runtime_error("不允许添加新的任务了");

			tasks.emplace([task]() { (*task)(); });
		}
		condition.notify_one();
		return res;
	}

	// 销毁线程池
	~ThreadPool() {
		{
			std::unique_lock<std::mutex> lock(queue_mutex);
			stop = true;
		}
		condition.notify_all();
		for (std::thread& worker : workers)
			worker.join();
	}

private:
	// 这些变量需要多个线程访问，因此我们需要互斥量保护他们
	std::vector< std::thread > workers; // 线程池中的线程
	std::queue< std::function<void()> > tasks; // 任务队列

	// 同步实现
	std::mutex queue_mutex;
	std::condition_variable condition;
	bool stop;//stop=true时表示运行

};
