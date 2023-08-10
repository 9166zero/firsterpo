#pragma once
#include<iostream>
#include<string>
#include<mysql/mysql.h>
#include <stdlib.h>
#include<chrono>
#include <map>
#include <sys/types.h>         
#include <sys/socket.h>
#include <netinet/in.h>
#include"mysqlclass.h"
#include"connectionpool.h"
#include<map>
#include"protocol.h"
#include<set>
using namespace std;
static bool usernamepd;
static bool hasphone;
//static ONLINEUSERIP oipinfo;//上线用户的ip等信息
typedef struct protocol_t
{
	char head[255];
	char body[255];
}PROTOCOL_T;
struct ThreadArgs
{
	//进入线程池的函数参数
	std::unique_ptr<int> fd;
	connectionpool* pool;
	int epoll_fd;
	USERINFO_T userinfo;
	SENDFILEINFO fileinfo;
	USERRISTER riginfo;
};
//上为原全局变量
class tcplogserver
{
public:
	tcplogserver() {};
	~tcplogserver() {};
	void tcpstart();
	void initnetwork();
	void waitforclient();
private:
	int socketfd = 0;
	int acceptfd = 0;
	pthread_t pthreadid;
	int len = 0;
	int pid = 0;
	int opt_val = 1;
	char buf[255] = { 0 };//存放客户端发过来的信息
	//处理连接后的业务

};

static void handleuserregister(std::unique_ptr<ThreadArgs> arg);//处理用户注册
static void falseuserquie(std::unique_ptr<ThreadArgs> arg);//处理产生错误的时候用户退出通知
static void handledownfile(std::unique_ptr<ThreadArgs> arg);//处理文件下载请求
static void handlefilereq(std::unique_ptr<ThreadArgs> arg);//处理文件收发请求的情况;
static void handlelog(std::unique_ptr<ThreadArgs> arg);//处理登录的情况;
static void handleuserquit(std::unique_ptr<ThreadArgs> arg);//处理用户退出的情况
static void* pthread_function(std::unique_ptr<ThreadArgs> arg);//前综合功能函数
static string logpd(connectionpool* pool, int begin, int end, string username, string password);//登录判断
static bool regpd(connectionpool* pool, int begin, int end, string username, string password, string phone);//注册判断
static bool adduserinfotosql(connectionpool* pool, int begin, int end, string username, string password, string phone);//添加用户sql

