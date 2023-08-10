#include"ThreadPool.h"
#include<iostream>
const int NUMBER = 2;
using namespace std;
ThreadPool::ThreadPool(int min, int max)
{
	do
	{
		minNum = min;
		maxNum = max;
		busyNum = 0;
		liveNum = min;
		exitNum = 0;

		shutdown = false;
		// this:���ݸ��߳���ں����Ĳ��������̳߳�
		managerID = thread(manager, this);

		threadIDs.resize(max);
		for (int i = 0; i < min; ++i)
		{
			threadIDs[i] = thread(worker, this);
		}
		return;
	} while (0);
}

ThreadPool::~ThreadPool()
{
	shutdown = true;
	//�������չ������߳�
	if (managerID.joinable()) managerID.join();
	//�����������������߳�
	cond.notify_all();
	for (int i = 0; i < maxNum; ++i)
	{
		if (threadIDs[i].joinable()) threadIDs[i].join();
	}
}

void ThreadPool::Add(Task t)
{
	unique_lock<mutex> lk(mutexPool);
	if (shutdown)
	{
		return;
	}
	//�������
	taskQ.push(t);
	cond.notify_all();
}

void ThreadPool::Add(callback f, void* a)
{
	unique_lock<mutex> lk(mutexPool);
	if (shutdown)
	{
		return;
	}
	//�������
	taskQ.push(Task(f, a));
	cond.notify_all();
}

int ThreadPool::Busynum()
{
	mutexPool.lock();
	int busy = busyNum;
	mutexPool.unlock();
	return busy;
}

int ThreadPool::Alivenum()
{
	mutexPool.lock();
	int alive = liveNum;
	mutexPool.unlock();
	return alive;
}

void ThreadPool::worker(void* arg)
{
	ThreadPool* pool = static_cast<ThreadPool*>(arg);
	// �������߳���Ҫ��ͣ�Ļ�ȡ�̳߳�������У�����ʹ��while
	while (true)
	{
		// ÿһ���̶߳���Ҫ���̳߳ؽ���������в���������̳߳��ǹ�����Դ����Ҫ����
		unique_lock<mutex> lk(pool->mutexPool);
		// ��ǰ��������Ƿ�Ϊ��
		while (pool->taskQ.empty() && !pool->shutdown)
		{
			// ����������������Ϊ0,�����̳߳�û�б��ر�,���赱ǰ�����߳�
			pool->cond.wait(lk);

			// �ж��Ƿ�Ҫ�����߳�,�������øù������߳���ɱ
			if (pool->exitNum > 0)
			{
				pool->exitNum--;
				if (pool->liveNum > pool->minNum)
				{
					pool->liveNum--;
					cout << "threadid: " << std::this_thread::get_id() << " exit......" << endl;
					// ��ǰ�߳�ӵ�л�������������Ҫ��������Ȼ������
					lk.unlock();
					return;
				}
			}
		}
		// �ж��̳߳��Ƿ�ر���
		if (pool->shutdown)
		{
			cout << "threadid: " << std::this_thread::get_id() << "exit......" << endl;
			return;
		}

		// �����������ȥ��һ������
		Task task = pool->taskQ.front();
		pool->taskQ.pop();
		pool->busyNum++;
		// ���������̳߳ض���ʱ���̳߳ؽ���
		lk.unlock();

		// ȡ��Task����󣬾Ϳ����ڵ�ǰ�߳���ִ�и�������
		cout << "thread: " << std::this_thread::get_id() << " start working..." << endl;
		task.function(task.arg);
		//(*task.function)(task.arg);
		free(task.arg);
		task.arg = nullptr;

		// ����ִ�����,æ�߳̽���
		cout << "thread: " << std::this_thread::get_id() << " end working..." << endl;
		lk.lock();
		pool->busyNum--;
		lk.unlock();
	}
}

// ����Ƿ���Ҫ����̻߳��������߳�
void ThreadPool::manager(void* arg)
{
	ThreadPool* pool = static_cast<ThreadPool*>(arg);
	// �������߳�Ҳ��Ҫ��ͣ�ļ����̳߳ض��к͹������߳�
	while (!pool->shutdown)
	{
		//ÿ��3����һ��
		//sleep(3);
		std::this_thread::sleep_for(std::chrono::seconds(3));

		// ȡ���̳߳�������������͵�ǰ�̵߳�����,����߳��п�����д���ݣ�����������Ҫ����
		// Ŀ������ӻ��������߳�
		unique_lock<mutex> lk(pool->mutexPool);
		int queuesize = pool->taskQ.size();
		int livenum = pool->liveNum;
		int busynum = pool->busyNum;
		lk.unlock();

		//����߳�
		//����ĸ���>�����̸߳��� && �����߳��� < ����߳���
		if (queuesize > livenum && livenum < pool->maxNum)
		{
			// ��Ϊ��forѭ���в������̳߳ر�����������Ҫ����
			lk.lock();
			// ���ڼ�������ӵ��̸߳���
			int count = 0;
			// ����߳�
			for (int i = 0; i < pool->maxNum && count < NUMBER && pool->liveNum < pool->maxNum; ++i)
			{
				// �жϵ�ǰ�߳�ID,�����洢�������߳�ID
				if (pool->threadIDs[i].get_id() == thread::id())
				{
					cout << "Create a new thread..." << endl;
					pool->threadIDs[i] = thread(worker, pool);
					// �̴߳������
					count++;
					pool->liveNum++;
				}
			}
			lk.unlock();
		}
		//�����߳�:��ǰ�����߳�̫����,�������߳�̫����
		//æ���߳�*2 < �����߳��� && �����߳��� >  ��С���߳���
		if (busynum * 2 < livenum && livenum > pool->minNum)
		{
			// �������̳߳�,��Ҫ����
			lk.lock();
			// һ������������
			pool->exitNum = NUMBER;
			lk.unlock();
			// �ù������߳���ɱ���޷�����ֱ��ɱ�������̣߳�ֻ��֪ͨ�����߳�������ɱ
			for (int i = 0; i < NUMBER; ++i)
				pool->cond.notify_all();  // �����߳���������������cond��
		}
	}
}


