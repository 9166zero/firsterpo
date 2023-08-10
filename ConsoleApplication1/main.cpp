#include<iostream>
#include<memory>
#include"mysqlclass.h"
#include"connectionpool.h"
#include<fstream>
#include <unistd.h>
#include"tcplogserver.h"
using namespace std;
//测试:单线程,使用或者不使用数据库连接池
//多线程同上
/*
void op1(int begin, int end)
{
	for (int i = begin; i < end; i++)
	{
		//创建一个新的连接
		mysqlclass conn;
		conn.connect("root", "916651466997", "demo", "192.168.11.128");
		char sql[1024] = { 0 };
		sprintf(sql, "insert into student values(%d,0520663,'某个人c','男','2023-08-15')", i);
		conn.updata(sql);
	}
}
void test1()
{
	//12640毫秒左右，插入5000条数据
	chrono::steady_clock::time_point begin = chrono::steady_clock::now();
	op1(0, 5000);
	chrono::steady_clock::time_point end = chrono::steady_clock::now();
	auto length = end - begin;
	cout << "非连接池,单线程,用时:" << length.count() << "纳秒"
		<< length.count() / 1000000 << "毫秒" << endl;

	//1519ms左右
	connectionpool* pool = connectionpool::getconnectpool();
	chrono::steady_clock::time_point begin = chrono::steady_clock::now();
	op2(pool, 0, 100);
	chrono::steady_clock::time_point end = chrono::steady_clock::now();
	auto length = end - begin;
	cout << "连接池,单线程,用时:" << length.count() << "纳秒"
		<< length.count() / 1000000 << "毫秒" << endl;
}

void test2()
{
	//多线程使用数据库访问的性能测试,非连接池 3309ms
	chrono::steady_clock::time_point begin = chrono::steady_clock::now();
	thread t1(op1, 0, 1000);
	thread t2(op1, 1000, 2000);
	thread t3(op1, 2000, 3000);
	thread t4(op1, 3000, 4000);
	thread t5(op1, 4000, 5000);
	t1.join();
	t2.join();
	t3.join();
	t4.join();
	t5.join();
	chrono::steady_clock::time_point end = chrono::steady_clock::now();
	auto length = end - begin;
	cout << "非连接池,多线程，用时:" << length.count() << "纳秒,"
		<< length.count() / 1000000 << "毫秒" << endl;

	//多线程使用数据库访问的性能测试, 连接池 530ms
	connectionpool* pool = connectionpool::getconnectpool();
	chrono::steady_clock::time_point begin = chrono::steady_clock::now();
	thread t1(op2, pool, 0, 1000);
	thread t2(op2, pool, 1000, 2000);
	thread t3(op2, pool, 2000, 3000);
	thread t4(op2, pool, 3000, 4000);
	thread t5(op2, pool, 4000, 5000);
	t1.join();
	t2.join();
	t3.join();
	t4.join();
	t5.join();
	chrono::steady_clock::time_point end = chrono::steady_clock::now();
	auto length = end - begin;
	cout << "连接池,多线程，用时:" << length.count() << "纳秒,"
		<< length.count() / 1000000 << "毫秒" << endl;
}
*/
/*
int query()
{
	mysqlclass conn;
	conn.connect("root", "916651466997", "demo", "192.168.11.128");
	string sql = "insert into student values(8,0520663,'某个人c','男','2023-08-15')";
	bool flag = conn.updata(sql);
	cout << "测试的结果:" << flag << endl;
	sql = "select * from student";
	conn.query(sql);
	cout << "conn.next()=" << conn.next() << endl;
	while (conn.next())
	{
		cout << conn.value(0) << ","
			<< conn.value(1) << ","
			<< conn.value(2) << ","
			<< conn.value(3) << ","
			<< conn.value(4) << "," << endl;
	}
	return 0;
}
*/

int main()
{
	/*
	客户端 -- 服务器端 -- 数据库
	数据库连接池在服务器端到数据库之间
	网络通信的服务端实际上就是数据库的客户端
	实际上是tcp通信
	tcp三次握手
	数据库服务器身份验证 用户名密码
	数据库服务器关闭连接的资源回收
	断开连接的TCP四次挥手

	使用数据库连接池可以频繁的实现上述连接不消耗太多资源
	数据库连接池类设置为单例模式类
	涉及共享资源的操作时要加锁
	*/
	//query();
	//test1();

	//printf("%s 向你问好!\n", "ConsoleApplication1");


	tcplogserver ts;
	ts.tcpstart();

	return 0;
}