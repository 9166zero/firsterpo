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
class mysqlclass
{
	//初始化数据库连接通过构造函数实现
	//释放数据库连接通过析构函数实现
public:
	mysqlclass();
	~mysqlclass();
	//连接数据库
	bool connect(string user, string passwd, string dbname, string ip, unsigned short port = 3306);
	//更新数据库: insert,update,delete
	bool updata(string sql);
	//查询数据库
	bool query(string sql);
	//遍历查询得到的结果集
	bool next();
	//得到结果集中的字段值
	string value(int index);
	//事务操作把事务设置成手动提交
	bool transaction();
	//提交事务
	bool commit();
	//事务回滚
	bool rollback();
	//刷新起始的空闲时间点
	void refreshAliveTime();
	//计算连接存活的总时长
	long long getAliveTime();
	//获得当前查询的结果
	MYSQL_RES* getresult();
	//检查密码是否匹配
	bool ispwdmtach(string& inPassword);
	//检查电话号码是否匹配
	bool isphonematch(string& inphone);
	//将搜索到的头像存储地址传出
	string getavatar();
private:
	//处理完结果集里的数据时释放内存
	void freeResult();
	MYSQL* m_conn = nullptr;
	MYSQL_RES* m_result = nullptr;
	MYSQL_ROW m_row = nullptr; //实际上是一个二级指针，指向了一个一级指针实际是一个char*的数组
	chrono::steady_clock::time_point m_alivetime;
};

