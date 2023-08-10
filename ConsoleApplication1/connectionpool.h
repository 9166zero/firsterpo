#pragma once
#include<iostream>
#include<mysql/mysql.h>
#include <stdlib.h>
#include<queue>
#include"mysqlclass.h"
#include<string>
#include<mutex>
#include<condition_variable>
#include<thread>
#include<chrono>
#include<memory>
using namespace std;
class connectionpool
{
	//设置成单例模式,懒汉模式只有实例化时才创建
public:
	//通过类名访问类的实例只能获取静态的函数或者变量
	//在c++11中使用静态的局部变量是线程安全的
	static connectionpool* getconnectpool();
	//防止对象被复制
	connectionpool(const connectionpool& obj) = delete;//删除拷贝构造函数，不再能被意外的调用
	connectionpool& operator=(const connectionpool& obj) = delete;
	shared_ptr<mysqlclass> getconnection();
	~connectionpool();

private:
	connectionpool();
	bool parsejsonFile();
	void produceConnection();//生产数据库连接
	void recycleConnection();//销毁数据库连接
	void addConnection();//生产连接的函数
	//信息
	string m_ip;
	string m_user;
	string m_passwd;
	string m_dbName;
	//unsigned short 
	unsigned short  m_port;
	int m_minsize;
	int m_maxsize;
	int m_timeout;
	int m_maxIdleTime;
	int connectnum;//记录连接数量
	//设置互斥锁防止多个线程访问队列出错
	mutex m_mutexQ;
	queue<mysqlclass*> m_connectQ;//任务队列
	condition_variable m_cond;

};

