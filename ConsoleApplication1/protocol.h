#pragma once
#ifndef PROTOCOL_H
#define PROTOCOL_H

#define LOGIN 1
#define CHAT 2
#define REGISTER 3
#define EXITLOG 4
#define LOGINFORM 5
#define INITLOGIN 6
#define FILEINFO 7
#define DOWNFILE 8
#define FINUPLOD 9
#include<string>
/*----------------------------请求头------------------------------*/
typedef struct head_t
{
	//业务类型 1登录，2聊天，3注册，4退出 ,5用户上线广播,6初始化在线用户列表，7文件信息 8下载文件
	int businessType; //9告诉服务器发送数据完成了
	int businessLength; //业务长度
}HEAD_T;

/*----------------------------请求体------------------------------*/

//聊天信息
typedef struct chatMsg_t
{
	char sengName[20]; //发送者
	//char receiveName[20]; //接收者
	//char message[300]; //发送的信息
}CHATMSG_T;


//用户信息
typedef struct userinfo_t
{
	char user_name[20];
	char user_pwd[20];
	char user_phone[20];
	//想要设置一个存储用户头像的变量
	unsigned char user_avatar[65536];
	unsigned int avatar_size;  // 存储实际头像数据的大小
}USERINFO_T;

/*----------------------------反馈头------------------------------*/
typedef struct feedback_t
{
	int businessType; //业务类型 1登录，2聊天，3注册，4退出 ,5用户上线广播,6初始化在线用户列表
	int businessLength; //业务长度
	int flag; //业务成功与否的状态
	int usernamepd;//判断用户名是否正确
}FEEDBACK_T;
/*-----------------------发送文件的信息------------------------------*/
typedef struct sendfileinfo
{
	char sengName[20]; //发送者
	char recName[20]; //接受者
	char filename[20];//文件名字
	int filesize;//文件大小
}SENDFILEINFO;
/*---------------------------用户注册信息------------------------------*/
typedef struct userrister_t
{
	char username[20]; //用户名
	char userphone[20]; //注册电话
	char userpwd[20]; //用户密码
}USERRISTER;
/*------------------------在线用户地址信息-----------------------------*/
typedef struct onlineuserip
{
	char ousername[20]; //在线用户名
	char oipstr[20];//在线的用户ip
	int oport; //在线的用户端口
	int ofd; //套接字编号
}ONLINEUSERIP;
/*---------------------------在线用户列表------------------------------*/
typedef struct onlineuserlist
{
	int count;//最多允许的在线用户数量
	ONLINEUSERIP userlist[100];//支持100个好友
}ONLINEUSERLIST;

#endif // PROTOCOL_H