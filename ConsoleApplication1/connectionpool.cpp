#include "connectionpool.h"
#include<jsoncpp/json/json.h>
#include<fstream>
#include<iostream>
using namespace Json;
connectionpool* connectionpool::getconnectpool()
{
	static connectionpool pool;
	return &pool;
}

bool connectionpool::parsejsonFile()
{
	ifstream ifs("/home/lvxiao/projects/ConsoleApplication1/bin/x64/Debug/dbconf.json");
	if (ifs.is_open()) {
		//cout << "ifs open success" << endl;
	}
	else
	{
		cout << "ifs open false" << endl;
	}
	Reader rd;
	Value root;
	rd.parse(ifs, root);
	cout << root.isObject() << endl;
	//cout << root << endl;
	if (root.isObject())//如果是json数据对象则将其全部读出来
	{
		m_ip = root["ip"].asString();
		m_port = (short unsigned int)root["port"].asInt();
		m_user = root["userName"].asString();
		m_passwd = root["password"].asString();
		m_dbName = root["dbName"].asString();
		m_minsize = root["minSize"].asInt();
		m_maxsize = root["maxSize"].asInt();
		m_maxIdleTime = root["maxIdleTime"].asInt();
		m_timeout = root["timeout"].asInt();
		/*cout << "m_ip= " << m_ip << endl;
		cout << "m_port= " << m_port << endl;
		cout << "m_user= " << m_user << endl;
		cout << "m_passwd= " << m_passwd << endl;
		cout << "m_dbName= " << m_dbName << endl;
		cout << "m_maxsize= " << m_maxsize << endl;
		cout << "m_maxIdleTime " << m_maxIdleTime << endl;
		cout << "m_timeout = " << m_timeout << endl;*/

		return true;
	}
	return false;
}


void connectionpool::produceConnection()//生产者线程
{
	while (true)
	{
		unique_lock<mutex> locker(m_mutexQ);//自动维护互斥锁出了作用域析构并释放锁
		//int temp = (int)m_connectQ.size();
		//int min = (int)m_minsize;
		while (m_connectQ.size() >= m_minsize)
		{
			m_cond.wait(locker);
		}
		addConnection();
		m_cond.notify_all();//唤醒所有消费者线程
	}
}

void connectionpool::recycleConnection()
{
	//回收线程的实现
	//unique_lock<mutex> locker(m_mutexQ);
	while (true)//周期性检测，定期休眠
	{
		this_thread::sleep_for(chrono::milliseconds(500));
		lock_guard<mutex> locker(m_mutexQ);
		while (m_connectQ.size() > m_minsize)
		{
			//队列式存储，故队头的是最先进入存储的，所以先判断其连接的时间
			//若该链接大于指定的空闲时长，将其释放
			mysqlclass* conn = m_connectQ.front();
			if (conn->getAliveTime() >= m_maxIdleTime)
			{
				m_connectQ.pop();
				connectnum--;
				delete conn;
			}
			else
			{
				break;
			}
		}
	}
}

void connectionpool::addConnection()
{
	mysqlclass* conn = new mysqlclass();
	conn->connect(m_user, m_passwd, m_dbName, m_ip, m_port);
	conn->refreshAliveTime();
	connectnum++;
	m_connectQ.push(conn);
}

shared_ptr<mysqlclass> connectionpool::getconnection()//消费者线程
{
	//cout << "getconnection is used" << endl;
	unique_lock<mutex> locker(m_mutexQ);
	while (m_connectQ.empty())
	{
		//cout << "go into the m_connectQ.empty()" << endl;
		cout << "in die while" << endl;
		if (cv_status::timeout == m_cond.wait_for(locker, chrono::milliseconds(m_timeout)))
		{
			//说明任务队列为空则阻塞在条件变量处 
			cout << "in die while" << endl;
			if (m_connectQ.empty())
			{
				continue;
			}
		}
	}

	shared_ptr<mysqlclass> connptr(m_connectQ.front(), [this](mysqlclass* conn) {
		lock_guard<mutex> locker(m_mutexQ);
		conn->refreshAliveTime();//任务队列，属于共享资源
		m_connectQ.push(conn);
		//cout << "add connect to taskqueue" << endl;
		});
	m_connectQ.pop();
	m_cond.notify_all();//既阻塞消费者又阻塞生产者
	return connptr;
}

connectionpool::~connectionpool()
{
	while (!m_connectQ.empty())
	{
		mysqlclass* conn = m_connectQ.front();
		m_connectQ.pop();
		delete conn;
	}
}

connectionpool::connectionpool()
{
	//cout << "connectionpool can use" << endl;
	//加载配置文件
	connectnum = 0;
	if (!parsejsonFile())
	{
		cout << "dir is false" << endl;
		return;
	}
	//初始化连接
	for (int i = 0; i < m_minsize; i++)
	{
		//连接可以被初始化
		addConnection();
		//cout << "for i =" << i << endl;
	}
	//创建两个线程，销毁过多的连接和增加过少的连接
	thread producer(&connectionpool::produceConnection, this);
	thread recycler(&connectionpool::recycleConnection, this);
	producer.detach();
	recycler.detach();

}


