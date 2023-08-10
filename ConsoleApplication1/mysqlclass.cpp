#include "mysqlclass.h"

mysqlclass::mysqlclass()
{
	m_conn = mysql_init(nullptr);
	mysql_set_character_set(m_conn, "utf8");
}

mysqlclass::~mysqlclass()
{
	if (m_conn != nullptr)
	{
		mysql_close(m_conn);
	}
	freeResult();
}

bool mysqlclass::connect(string user, string passwd, string dbname, string ip, unsigned short port)
{
	MYSQL* ptr = mysql_real_connect(m_conn, ip.c_str(), user.c_str(), passwd.c_str(), dbname.c_str(), port, nullptr, 0);

	return ptr != nullptr;
}

bool mysqlclass::updata(string sql)
{
	if (mysql_query(m_conn, sql.c_str()))//该函数执行成功会返回0，失败会返回非0
	{
		return false;
	}

	return true;
}

bool mysqlclass::query(string sql)
{
	//清空上次的内存
	freeResult();

	//传递一个select语句
	if (mysql_query(m_conn, sql.c_str()))//
	{
		return false;
	}
	m_result = mysql_store_result(m_conn);
	return true;
}

bool mysqlclass::next()
{
	if (m_result != nullptr)
	{
		m_row = mysql_fetch_row(m_result);
		if (m_row != nullptr)
		{
			return true;
		}
	}
	return false;
}

string mysqlclass::value(int index)
{
	int colcount = mysql_num_fields(m_result);//列数
	m_row = mysql_fetch_row(m_result);
	if (index >= colcount || index < 0)
	{
		cout << "输入了一个无效索引" << endl;
		return string();
	}
	cout << "colcount = " << colcount << endl;
	if (m_row == nullptr)
	{
		cout << "m_row==null" << endl;
		return "false";
	}
	//cout << "m_row=" << m_row << endl;
	char* val = m_row[index];
	unsigned long  length = mysql_fetch_lengths(m_result)[index];
	//返回当前字段所有对应的值的长度,返回一个unsignlong的数组,选index为对应的那个

	return string(val, length);

}

bool mysqlclass::transaction()
{
	return mysql_autocommit(m_conn, false);
}

bool mysqlclass::commit()
{
	return mysql_commit(m_conn);
}

bool mysqlclass::rollback()
{

	return mysql_rollback(m_conn);

}

void mysqlclass::refreshAliveTime()
{
	m_alivetime = chrono::steady_clock::now();
}

long long mysqlclass::getAliveTime()
{
	chrono::nanoseconds res = chrono::steady_clock::now() - m_alivetime;
	chrono::milliseconds millsec = chrono::duration_cast<chrono::milliseconds>(res);
	return millsec.count();
}

MYSQL_RES* mysqlclass::getresult()
{
	return m_result;
}

bool mysqlclass::ispwdmtach(string& inPassword)
{
	//cout << "ispwdmatch开始了" << endl;
	string passwordColumn = value(2); // 假设passwordIndex是password字段在查询结果中的索引
	//cout << "value(2)= " << passwordColumn << endl;
	if (passwordColumn == inPassword) {
		return true;
	}
	return false;
}

bool mysqlclass::isphonematch(string& inphone)
{
	string phonewordColumn = value(0); // 假设passwordIndex是password字段在查询结果中的索引
	//cout << "value(0)= " << phonewordColumn << endl;
	if (phonewordColumn == inphone) {
		return true;
	}
	return false;
}

string mysqlclass::getavatar()
{
	//cout << "getavatar开始了" << endl;
	string avataraddress = value(4);
	//cout << "value(4)=" << avataraddress << endl;
	return avataraddress;
}

void mysqlclass::freeResult()
{
	if (m_result)//若没有指向一块有效内存则无事发生
	{
		mysql_free_result(m_result);
		m_result = nullptr;
	}
}
