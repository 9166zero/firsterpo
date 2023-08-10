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
	// ���캯���������̳߳�
	ThreadPool(size_t threads) : stop(false) {
		for (size_t i = 0; i < threads; ++i)
			workers.emplace_back(
				[this]
				{
					for (;;)//�߳�����ѭ���ȴ�����
					{
						std::function<void()> task;//����һ������ֵΪvoid�ĺ�������
						{
							//������������ֹ����߳�ͬʱ�޸��������
							std::unique_lock<std::mutex> lock(this->queue_mutex);
							//��stopΪtrue������������������ʱ�����߳�
							//��û������ִ�е�ʱ��Ҫռ��cpu��Դ
							//������������ʹ�ó�����ͣ
							this->condition.wait(lock,
								[this] { return this->stop || !this->tasks.empty(); });
							//���stop����Ϊtrue���������Ϊ�գ���ô��������ͷ��أ��߳̾ͽ�����
							if (this->stop && this->tasks.empty())  return;
							//�����������ȡ��һ������std::move()�������ڽ�����Ӷ������Ƴ����Ա��⸴��������󣩣�
							//Ȼ��pop()�����Ƴ������е��������
							task = std::move(this->tasks.front());
							this->tasks.pop();
						}
						task();
					}
				}
				);
	}
	// ��������̳߳�

	template<class F, class... Args>
	auto enqueue(F&& f, Args&&... args)//β��������
		-> std::future<typename std::result_of<F(Args...)>::type>
	{
		//�����ƶ��������ķ��ؽ��
		using return_type = typename std::result_of<F(Args...)>::type;
		//��ʹ�� std::forward<F>(f) ����ʵ������ת����ȷ�� f �ڱ� std::bind ����ʱ������ԭʼ��ֵ���
		//�����ȿ��Ա�֤���ܣ��ֿ�����ߺ�����ͨ���ԣ�ʹ�������Խ����������͵Ĳ�����
		//��ģ����� return_type() ��ʾ�洢�� packaged_task �еĺ����ķ������͡�
		auto task = std::make_shared< std::packaged_task<return_type()> >(
			std::bind(std::forward<F>(f), std::forward<Args>(args)...)
			);

		std::future<return_type> res = task->get_future();
		{
			std::unique_lock<std::mutex> lock(queue_mutex);//RAII��ʵ��

			// �̳߳�ֹͣʱ����ֹ����µ�����
			if (stop)
				throw std::runtime_error("����������µ�������");

			tasks.emplace([task]() { (*task)(); });
		}
		condition.notify_one();
		return res;
	}

	// �����̳߳�
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
	// ��Щ������Ҫ����̷߳��ʣ����������Ҫ��������������
	std::vector< std::thread > workers; // �̳߳��е��߳�
	std::queue< std::function<void()> > tasks; // �������

	// ͬ��ʵ��
	std::mutex queue_mutex;
	std::condition_variable condition;
	bool stop;//stop=trueʱ��ʾ����

};
