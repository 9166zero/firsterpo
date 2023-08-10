#include "tcplogserver.h"
#include<iostream>
#include<memory>
#include<fstream>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstring>
#include"protocol.h"
#include <sys/epoll.h>
#include"ThreadPool.h"
#include <sys/ioctl.h>
#include<unordered_set>
using namespace std;
map<string, ONLINEUSERIP> onlinemap;//在线用户容器，根据用户名记录登录信息
map<int, string> recorduser;//根据套接字记录用户名在线容器，只负责记录和错误处理
std::mutex mtx;  // 全局互斥锁
unordered_set<int> processing_fds;  // 使用一个无序集合来存储正在处理的文件描述符
void tcplogserver::tcpstart()
{
	initnetwork();
}
void tcplogserver::initnetwork()
{
	//初始化网络  参数一：使用ipv4  参数二：流式传输
	socketfd = socket(AF_INET, SOCK_STREAM, 0);
	if (socketfd == -1)
	{
		perror("socket error");
		cout << "socket error 创建套接字失败!" << endl;
	}
	else
	{
		//cout << "socket success 创建套接字成功!" << endl;
		//原本使用struct sockaddr，通常使用sockaddr_in更为方便，两个数据类型是等效的，可以相互转换
		struct sockaddr_in s_addr; //ipv4地址结构
		//确定使用哪个协议族  ipv4
		s_addr.sin_family = AF_INET;

		//系统自动获取本机ip地址 也可以是本地回环地址：127.0.0.1
		inet_pton(AF_INET, "192.168.11.137", &(s_addr.sin_addr.s_addr));
		char ipAddress[INET_ADDRSTRLEN];
		const char* resip = inet_ntop(AF_INET, &(s_addr.sin_addr), ipAddress, INET_ADDRSTRLEN);
		//cout << "本机ip地址为:" << ipAddress << endl;

		//端口一个计算机有65535个  10000以下是操作系统自己使用的，  自己定义的端口号为10000以后
		s_addr.sin_port = htons(12345);  //自定义端口号为12345

		len = sizeof(s_addr);

		//端口复用 解决 address already user 问题
		setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, (const void*)opt_val, sizeof(opt_val));

		//绑定ip地址和端口号
		int res = bind(socketfd, (struct sockaddr*)&s_addr, len);
		if (res == -1)
		{
			perror("bind error");
		}
		else
		{
			//监听这个地址和端口有没有客户端来连接  第二个参数现在没有用  只要大于0就行

			if (listen(socketfd, 2) == -1)
			{
				perror("listen error");
			}
			waitforclient();
		}
	}
}

void tcplogserver::waitforclient()
{
	cout << "进入到了waitforclient" << endl;
	connectionpool* pool = connectionpool::getconnectpool();  //创建唯一的数据库连接池
	ThreadPool threadpool(5);  // 创建线程池，这里我们创建了5个工作线程

	// 创建一个epoll实例
	int epoll_fd = epoll_create1(0);
	if (epoll_fd == -1) {
		std::cerr << "创建epoll实例失败了" << std::endl;
		return;
	}

	// 添加服务器监听socket到epoll监听队列中
	struct epoll_event event;
	event.events = EPOLLIN;
	event.data.fd = socketfd;
	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, socketfd, &event)) {
		std::cerr << "增加epoll失败了" << std::endl;
		close(epoll_fd);
		return;
	}

	// 用于保存epoll返回的事件的数组
	struct epoll_event events[10];
	while (1) {//死循环监听客户端的连接请求
		// 等待epoll事件
		int num_events = epoll_wait(epoll_fd, events, 10, -1);
		if (num_events == -1) {
			std::cerr << "等待 epoll 事件失败" << std::endl;
			break;
		}

		// 遍历所有的事件
		for (int i = 0; i < num_events; ++i) {
			if (events[i].events & EPOLLERR) {
				std::cerr << "文件描述符上的 epoll 错误,连接断开 " << events[i].data.fd << std::endl;
				std::unique_ptr<ThreadArgs> arg(new ThreadArgs());
				arg->fd = std::unique_ptr<int>(new int(events[i].data.fd));
				arg->pool = pool;
				arg->epoll_fd = epoll_fd;

				falseuserquie(std::move(arg)); // 处理退出的函数
				//close(events[i].data.fd);
			}
			else if (events[i].data.fd == socketfd) {
				// 有一个新的连接
				//cout << "正在等待用户连接" << endl;
				int client_fd = accept(socketfd, NULL, NULL);
				if (client_fd == -1) {
					if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
						// 我们已经处理了所有的连接
						break;
					}
					else {
						std::cerr << "接受客户端的连接失败了" << std::endl;
						continue;
					}
				}
				// 将新的fd添加到epoll实例中
				event.data.fd = client_fd;
				if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event)) {
					std::cerr << "将文件描述添加进入epoll失败了" << std::endl;
					close(client_fd);
				}
			}
			else
			{
				// 其他的已连接 socket 有数据可读
				//cout << "检测到用户发送消息" << endl;
				int client_fd = events[i].data.fd;
				std::unique_lock<std::mutex> lock(mtx);
				//当子线程某套接字存在io操作时，主线程检测到但是不处理

				//cout << "调用了unique_lock" << endl;
				if (processing_fds.find(client_fd) == processing_fds.end()) {
					mtx.unlock();
					//cout << "解开了unique_lock" << endl;
					//cout << "收到了fd=" << client_fd << "发来的请求正在解析" << endl;
					// 这个连接没有正在被处理，所以我们可以处理它
					// 先读取请求头
					HEAD_T head = {  };
					//int bytes_available;
					//ioctl(client_fd, FIONREAD, &bytes_available);
					//cout << "收到了fd=" << client_fd << "开始read" << endl;
					read(client_fd, &head, sizeof(HEAD_T));

					//cout << "收到了fd=" << client_fd << "发来的请求read" << endl;
					// 根据 head 处理
					if (head.businessType == LOGIN) {//处理登录请求
						USERINFO_T user = {  };//协议体
						cout << "head.businessLength=" << head.businessLength << endl;
						char* buffer = (char*)&user;
						int totalReceived = 0;
						while (totalReceived < 65600) {
							int bytesReceived = recv(client_fd, buffer + totalReceived, 65600 - totalReceived, 0);
							if (bytesReceived <= 0) {
								// handle error or closed connection
								break;
							}
							totalReceived += bytesReceived;
						}
						//int res = read(client_fd, &user, head.businessLength);
						cout << "totalReceived大小为:" << totalReceived << endl;
						cout << "用户密码：" << user.user_pwd << "用户名：" << user.user_name << endl;
						std::unique_ptr<ThreadArgs> arg(new ThreadArgs());
						arg->fd = std::unique_ptr<int>(new int(client_fd));
						arg->pool = pool;
						arg->epoll_fd = epoll_fd;
						arg->userinfo = user; // 这里将 user 对象赋值给 arg->userinf
						cout << "登录的fd=" << client_fd << "epoll_fd=" << epoll_fd << endl;
						// 添加处理登录的任务到线程池
						threadpool.enqueue([client_fd, pool, epoll_fd, user] {
							std::unique_ptr<ThreadArgs> arg(new ThreadArgs());
							arg->fd = std::unique_ptr<int>(new int(client_fd));
							arg->pool = pool;
							arg->epoll_fd = epoll_fd;
							arg->userinfo = user; // 这里将 user 对象赋值给 arg->userinf
							cout << "登录的fd=" << client_fd << "epoll_fd=" << epoll_fd << endl;
							handlelog(std::move(arg)); // 处理登录的函数
							});
					}
					// 添加其他处理业务类型的代码...
					if (head.businessType == EXITLOG) {//处理退出请求
						USERINFO_T user = {  };//协议体
						int res = read(client_fd, &user, head.businessLength);
						cout << "res大小为:" << res << endl;
						cout << "检测到用户退出请求" << "用户名：" << user.user_name << endl;
						// 添加处理登录的任务到线程池
						std::unique_lock<std::mutex> lock(mtx);
						processing_fds.insert(client_fd);//将其添加到正在处理的集合中
						mtx.unlock();
						threadpool.enqueue([client_fd, pool, epoll_fd, user] {
							std::unique_ptr<ThreadArgs> arg(new ThreadArgs());
							arg->fd = std::unique_ptr<int>(new int(client_fd));
							arg->pool = pool;
							arg->epoll_fd = epoll_fd;
							arg->userinfo = user; // 这里将 user 对象赋值给 arg->userinf
							handleuserquit(std::move(arg)); // 处理退出的函数
							});
					}
					if (head.businessType == FILEINFO) {//文件发送请求
						cout << "收到了用户的文件发送请求" << endl;
						SENDFILEINFO tempfileinfo = { };//协议体
						int res = read(client_fd, &tempfileinfo, head.businessLength);
						cout << "res大小为:" << res << endl;
						cout << "检测到用户:" << tempfileinfo.sengName << "发送文件:" << tempfileinfo.filename
							<< "文件大小:" << tempfileinfo.filesize << "给用户:" << tempfileinfo.recName << endl;
						// 添加文件收发的任务到线程池
						threadpool.enqueue([client_fd, pool, epoll_fd, tempfileinfo] {
							std::unique_ptr<ThreadArgs> arg(new ThreadArgs());
							arg->fd = std::unique_ptr<int>(new int(client_fd));
							arg->pool = pool;
							arg->epoll_fd = epoll_fd;
							arg->fileinfo = tempfileinfo;
							handlefilereq(std::move(arg)); // 处理文件收发的函数
							});
					}
					if (head.businessType == DOWNFILE) {//文件下载请求
						cout << "收到了用户的文件下载请求" << endl;
						SENDFILEINFO tempfileinfo = { };//协议体
						int res = read(client_fd, &tempfileinfo, head.businessLength);
						cout << "res大小为:" << res << endl;
						cout << "检测到用户:" << tempfileinfo.recName << "下载 文件:" << tempfileinfo.filename
							<< "文件大小:" << tempfileinfo.filesize << "来自用户:" << tempfileinfo.sengName << endl;
						// 添加文件收发的任务到线程池
						threadpool.enqueue([client_fd, pool, epoll_fd, tempfileinfo] {
							std::unique_ptr<ThreadArgs> arg(new ThreadArgs());
							arg->fd = std::unique_ptr<int>(new int(client_fd));
							arg->pool = pool;
							arg->epoll_fd = epoll_fd;
							arg->fileinfo = tempfileinfo;
							handledownfile(std::move(arg)); // 处理文件收发的函数
							});
					}
					if (head.businessType == REGISTER) {//用户注册
						cout << "收到了用户的注册请求" << endl;
						USERRISTER  regiserinfo = {};//协议体
						int res = read(client_fd, &regiserinfo, head.businessLength);
						cout << "res大小为:" << res << endl;
						/*cout << "检测到用户注册名:" << regiserinfo.username << "用户注册电话:" << regiserinfo.userphone
							<< "用户注册密码" << regiserinfo.userpwd << endl;*/
							// 添加文件收发的任务到线程池
						threadpool.enqueue([client_fd, pool, epoll_fd, regiserinfo] {
							std::unique_ptr<ThreadArgs> arg(new ThreadArgs());
							arg->fd = std::unique_ptr<int>(new int(client_fd));
							arg->pool = pool;
							arg->epoll_fd = epoll_fd;
							arg->riginfo = regiserinfo;
							handleuserregister(std::move(arg)); // 处理文件收发的函数
							});
					}
					if (head.businessType == FINUPLOD)
					{
						cout << "用户上传文件完毕了" << endl;
					}
					cout << "收到了fd=" << client_fd << "发来的请求正在解析到if最后了" << endl;
				}
				//标记判断fd是否正在被处理

			}
		}
	}
}

void handleuserregister(std::unique_ptr<ThreadArgs> arg)
{
	std::unique_lock<std::mutex> lock(mtx);
	//不该过早释放它导致主线程阻塞  
	cout << "收到了下载请求" << endl;
	// 获取文件描述符
	int fd = *(arg->fd);
	processing_fds.insert(fd);//将其添加到正在处理的集合
	//先向客户端发送反馈头告知开始下载
	FEEDBACK_T back = {};//协议头
	USERRISTER risinfo = { };//协议体
	risinfo = arg->riginfo;
	string username;
	string userpwd;
	string phone;
	username = risinfo.username;
	userpwd = risinfo.userpwd;
	phone = risinfo.userphone;
	connectionpool* pool = arg->pool;
	cout << "username = " << username << " userpwd= " << userpwd << "phone = " << phone << endl;
	cout << "将要执行注册业务" << endl;
	if (regpd(pool, 0, 1, username, userpwd, phone))
	{
		//cout << "regpd返回正确，判断已经存在此电话号码" << endl;
		back.flag = -1;
	}
	else
	{
		//cout << "logpd返回错误，说明可以用这个电话号码注册" << endl;
		back.flag = 1;
		//将该用户的信息写入数据库
		bool a = adduserinfotosql(pool, 0, 1, username, userpwd, phone);
	}
	back.businessType = REGISTER;
	back.businessLength = sizeof(USERINFO_T);
	write(fd, &back, sizeof(FEEDBACK_T));
	sleep(0.5);
	processing_fds.erase(fd);//将其从集合中移除
	mtx.unlock();
	return;
}

void handledownfile(std::unique_ptr<ThreadArgs> arg)
{
	std::unique_lock<std::mutex> lock(mtx);
	//不该过早释放它导致主线程阻塞  
	cout << "收到了下载请求" << endl;
	// 获取文件描述符
	int fd = *(arg->fd);
	processing_fds.insert(fd);//将其添加到正在处理的集合
	//先向客户端发送反馈头告知开始下载
	FEEDBACK_T back = {};//协议头
	SENDFILEINFO fileinfo = { };//协议体
	fileinfo = arg->fileinfo;
	back.businessLength = sizeof(SENDFILEINFO);//发送下载文件许可
	back.businessType = DOWNFILE;
	// 创建一个缓冲区
	char buffer[sizeof(FEEDBACK_T) + sizeof(SENDFILEINFO)];
	memcpy(buffer, &back, sizeof(FEEDBACK_T));
	memcpy(buffer + sizeof(FEEDBACK_T), &fileinfo, sizeof(SENDFILEINFO));
	write(fd, &buffer, sizeof(buffer));//发给接收者
	cout << "发送了下载许可" << endl;
	sleep(1);//等待1s给他反应
	// 根据arg中的信息创建文件名
	char filename[100]; // 存储文件名
	snprintf(filename, sizeof(filename), "tempfile/%s_%s_%s",
		arg->fileinfo.sengName, arg->fileinfo.recName, arg->fileinfo.filename);
	cout << "输出读取文件名" << filename << endl;

	// 打开文件
	FILE* fp = fopen(filename, "rb");
	if (fp == NULL)
	{
		perror("打开文件失败");
		return;
	}
	else
	{
		cout << "打开文件成功" << endl;
	}

	// 分块读取文件数据并发送
	char buffersend[65536];
	int bytesRead = 0;
	while ((bytesRead = fread(buffersend, 1, sizeof(buffersend), fp)) > 0)
	{
		// 将读取到的数据写入套接字
		if (write(fd, buffersend, bytesRead) == -1)
		{
			perror("分块发送文件失败");
			break;
		}
	}
	// 关闭文件
	fclose(fp);
	cout << "客户端下载文件完毕" << endl;
	processing_fds.erase(fd);//将其从集合中移除
	mtx.unlock();
	return;
}

void handlefilereq(std::unique_ptr<ThreadArgs> arg)
{
	std::unique_lock<std::mutex> lock(mtx);
	cout << "进入到了处理文件收发的函数" << endl;
	// 获取文件描述符
	int fd = *(arg->fd);
	processing_fds.insert(fd);//将其添加到正在处理的集合中
	// 根据业务长度，决定要读取的文件大小
	int filesize = arg->fileinfo.filesize;
	cout << "要读取的文件大小为:" << filesize << endl;

	// 打开或创建一个文件，用于保存接收到的文件数据
	char filename[100]; // 你可能需要根据你的实际需求调整这个大小
	snprintf(filename, sizeof(filename), "tempfile/%s_%s_%s", arg->fileinfo.sengName,
		arg->fileinfo.recName, arg->fileinfo.filename);
	cout << "输出保存文件名" << filename;
	FILE* fp = fopen(filename, "wb");
	if (fp == NULL)
	{
		perror("Failed to open file");
		lock.unlock();  // 在函数的正常退出处解锁
		return;
	}
	// 分块读取文件数据
	char buffer[65536];
	int bytesRead = 0;
	int totalBytesRead = 0;
	while (totalBytesRead < filesize)
	{
		//cout << "处于循环当中" << endl;
		bytesRead = read(fd, buffer, sizeof(buffer));
		if (bytesRead <= 1)
		{
			cout << "数据发送完了，直接提前退出" << endl;
			break;
		}
		if (bytesRead == -1)
		{
			perror("读取文件失败");
			break;
		}
		if (bytesRead == 0)
		{
			perror("服务器关闭了连接");
			break;
		}

		// 将读取到的数据写入文件
		fwrite(buffer, bytesRead, 1, fp);
		totalBytesRead += bytesRead;
	}
	cout << "要读取的文件完毕" << filesize << endl;
	// 关闭文件
	fclose(fp);


	//向接受者发送接收广播
	FEEDBACK_T head = {};//反馈头
	SENDFILEINFO tempfileinfo = { };//协议体
	tempfileinfo = arg->fileinfo;
	head.businessLength = sizeof(SENDFILEINFO);
	head.businessType = FILEINFO;
	head.flag = 1;
	string recusername = tempfileinfo.recName;
	cout << "接受者为:" << recusername << endl;
	auto it = onlinemap.find(recusername);
	if (it != onlinemap.end())
	{
		cout << "找到了这个用户" << it->first << "现在向其发送是否要接受文件的通知" << endl;
		int recfd = it->second.ofd;
		// 创建一个缓冲区
		char buffer[sizeof(FEEDBACK_T) + sizeof(SENDFILEINFO)];
		memcpy(buffer, &head, sizeof(FEEDBACK_T));
		memcpy(buffer + sizeof(FEEDBACK_T), &tempfileinfo, sizeof(SENDFILEINFO));
		write(recfd, buffer, sizeof(buffer));
		sleep(0.5);
		cout << "发送接收文件消息完毕" << endl;
	}
	else
	{
		cout << "没有这样的用户或者用户已经下线了" << endl;
	}
	processing_fds.erase(fd);
	// 在移除之后打印所有的元素
	std::cout << "processing_fds应该不存在fd=  " << fd;
	for (const auto& fd : processing_fds) {
		std::cout << "找到的fd= " << fd << " ";
	}
	lock.unlock();  // 在函数的正常退出处解锁
	cout << "运行到了解锁后" << endl;
	return;
}//我可以只在上传文件的代码上使用异步Io把，这是我上传文件的代码:

void handlelog(std::unique_ptr<ThreadArgs> arg)
{
	std::unique_lock<std::mutex> lock(mtx);
	int fd = *(arg->fd);//客户端连接服务器后的文件描述符
	processing_fds.insert(fd);//将其添加到正在处理的集合中
	cout << "开始了这次用户登录业务" << endl;
	cout << "在线人数：" << onlinemap.size() << endl;
	connectionpool* pool = arg->pool;


	// 获取客户端地址信息
	struct sockaddr_in clientAddr;
	socklen_t addrLen = sizeof(clientAddr);
	getpeername(fd, (struct sockaddr*)&clientAddr, &addrLen);
	// 将地址信息转换为IP地址字符串
	char ipStr[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &(clientAddr.sin_addr), ipStr, INET_ADDRSTRLEN);
	// 获取端口号
	int port = ntohs(clientAddr.sin_port);
	// 打印IP地址和端口号
	//cout << "客户端IP地址:" << ipStr << endl;
	//cout << "客户端端口号：" << port << endl;

	ONLINEUSERIP oipinfo = {  };//上线用户的ip等信息
	HEAD_T head = {  };//协议头
	USERINFO_T user = {  };//协议体
	CHATMSG_T buf = {  };//协议体
	FEEDBACK_T back = {  };//反馈体
	USERRISTER risinfo = {  };//用户注册信息
	user = arg->userinfo;
	int res = 0;
	//读取的数据应该在外面就完全读取完毕
	//int res = read(fd, &user, head.businessLength);
	if (res == 0)
	{
		cout << "能进入res判断" << endl;
		string username;
		string userpwd;

		username = user.user_name;
		userpwd = user.user_pwd;

		cout << "username = " << username << " userpwd= " << userpwd << endl;
		//cout << "将要执行登录业务" << endl;
		string tempresult = logpd(pool, 0, 1, username, userpwd);//用于存放在数据库中的搜索结果
		if (tempresult != "false")
		{
			cout << "logpd返回正确，继续执行" << endl;
			back.flag = 1;
			char* charPtr = const_cast<char*>(username.c_str());
			char* searchKey = charPtr;
			oipinfo.ofd = fd;
			oipinfo.oport = port;
			int afd = fd;
			cout << "result=" << tempresult << endl;
			// 打开文件
			FILE* fp = fopen(tempresult.c_str(), "rb");
			if (fp == NULL)
			{
				perror("打开文件失败");
				return;
			}
			else
			{
				cout << "打开文件成功" << endl;
			}
			// 获取文件大小
			fseek(fp, 0, SEEK_END);
			long fileSize = ftell(fp);
			fseek(fp, 0, SEEK_SET);

			if (fileSize > sizeof(userinfo_t::user_avatar))
			{
				cerr << "文件过大，无法存储在结构体中!" << endl;
				fclose(fp);
				return;
			}
			cout << "读取filesize大小为:" << fileSize << endl;
			// 读取文件内容到结构体的user_avatar字段中
			size_t bytesRead = fread(user.user_avatar, 1, fileSize, fp);
			if (bytesRead != fileSize)
			{
				cerr << "读取文件时出错!" << endl;
				fclose(fp);
				return;
			}
			user.avatar_size = bytesRead;  // 更新实际读取的大小
			cout << "读取数据大小为:" << user.avatar_size << endl;
			fclose(fp);


			// 将字符串复制到字符数组中
			strncpy(oipinfo.ousername, username.c_str(), sizeof(oipinfo.ousername) - 1);
			oipinfo.ousername[sizeof(oipinfo.ousername) - 1] = '\0';  // 确保以空字符结尾
			cout << "oipinusername =" << oipinfo.ousername << endl;
			strncpy(oipinfo.oipstr, ipStr, sizeof(oipinfo.oipstr) - 1);
			oipinfo.oipstr[sizeof(oipinfo.oipstr) - 1] = '\0';  // 确保以空字符结尾
			//更新在线用户列表
			onlinemap.insert(make_pair(charPtr, oipinfo));//容器插入数据
			recorduser.insert(make_pair(fd, username));//根据套接字记录用户名
			cout << "在线人数：" << onlinemap.size() << endl;
		}
		else
		{
			cout << "logpd返回错误" << endl;
			if (usernamepd == true)
			{
				back.usernamepd = 1;
			}
			else
			{
				back.usernamepd = 0;
			}
			back.flag = -1;
			back.businessType = LOGIN;
			back.businessLength = sizeof(USERINFO_T);
			write(fd, &back, sizeof(FEEDBACK_T));
			sleep(1);
			lock.unlock();
			return;
		}
		back.businessType = LOGIN;
		back.businessLength = sizeof(USERINFO_T);
		// 创建发送用户信息的缓冲区
		char bufferuser[sizeof(FEEDBACK_T) + sizeof(USERINFO_T)];
		memcpy(bufferuser, &back, sizeof(FEEDBACK_T));
		memcpy(bufferuser + sizeof(FEEDBACK_T), &user, sizeof(USERINFO_T));
		cout << "发送的bufferuser大小为" << sizeof(bufferuser) << endl;
		write(fd, &bufferuser, sizeof(bufferuser));
		sleep(1);
		//cout << "数据发送成功" << endl;
		//处理广播用户登录成功
		FEEDBACK_T backlogsuccess = {  };//登录成功的反馈
		backlogsuccess.businessType = LOGINFORM;
		backlogsuccess.flag = 1;
		backlogsuccess.businessLength = sizeof(ONLINEUSERIP);
		// 创建一个缓冲区
		char buffer[sizeof(FEEDBACK_T) + sizeof(ONLINEUSERIP)];
		memcpy(buffer, &backlogsuccess, sizeof(FEEDBACK_T));
		memcpy(buffer + sizeof(FEEDBACK_T), &oipinfo, sizeof(ONLINEUSERIP));
		//write(fd, &buffer, sizeof(buffer));//发给发送者现在不需要了
		//sleep(1);
		//向所有其他用户广播该用户上线
		for (auto it = onlinemap.begin(); it != onlinemap.end(); it++)
		{
			string suname = it->second.ousername;
			if (suname != username) {
				int sendfd = it->second.ofd;
				int port = it->second.oport;
				string ipstr = it->second.oipstr;
				/*cout << "sendfd =" << sendfd << endl;
				cout << "suname =" << suname << endl;
				cout << "ipstr =" << ipstr << endl;
				cout << "port = " << port << endl;*/
				cout << "收到用户 " << username << " 登录上线的消息，并转发给用户 " << suname << endl;
				write(sendfd, &buffer, sizeof(buffer));//发给接收者
				sleep(0.5);
			}
		}
		//向刚登陆的用户发送在线用户列表2.0
		ONLINEUSERLIST userList = { };
		int index = 0;
		userList.count = onlinemap.size();
		for (const auto& pair : onlinemap)
		{
			if (index >= 100)
			{
				// 若容器中的元素数量超过了上限，在这里进行适当的错误处理
				cout << "容器中的元素过多" << endl;
				break;
			}
			// 将容器中的信息复制到结构体中
			strncpy(userList.userlist[index].ousername, pair.first.c_str(), sizeof(userList.userlist[index].ousername) - 1);
			strncpy(userList.userlist[index].oipstr, pair.second.oipstr, sizeof(userList.userlist[index].oipstr) - 1);
			userList.userlist[index].oport = pair.second.oport;
			userList.userlist[index].ofd = pair.second.ofd;
			index++;
		}
		char buffers[sizeof(FEEDBACK_T) + sizeof(ONLINEUSERLIST)];
		char* ptr = buffers;
		// 填充反馈头信息
		FEEDBACK_T feedback;
		feedback.businessType = INITLOGIN; // 初始化在线用户列表
		feedback.businessLength = sizeof(ONLINEUSERLIST);
		feedback.flag = 1; // 成功标志
		memcpy(ptr, &feedback, sizeof(FEEDBACK_T));
		ptr += sizeof(FEEDBACK_T);
		//cout << "填充用户信息前测试在线用户列表的用户名值:" << a << endl;
		// 填充用户列表信息
		memcpy(ptr, &userList, sizeof(ONLINEUSERLIST));
		// 发送数据到Qt服务端
		write(fd, buffers, sizeof(buffers));
		sleep(1);
	}
	else
	{
		cout << "数据丢包" << endl;
	}
	cout << "结束了这次用户登录业务" << endl;
	processing_fds.erase(fd);
	// 在移除之后打印所有的元素
	std::cout << "processing_fds应该不存在fd=  " << fd;
	for (const auto& fd : processing_fds) {
		std::cout << "找到的fd= " << fd << " ";
	}
	lock.unlock();
	return;
}

void handleuserquit(std::unique_ptr<ThreadArgs> arg)
{
	std::unique_lock<std::mutex> lock(mtx);
	cout << "开始了这次用户退出业务" << endl;
	int fd = *(arg->fd);//客户端连接服务器后的文件描述符
	//processing_fds.insert(fd);//将其添加到正在处理的集合中
	std::cout << "processing_fds应该存在fd=  " << fd;
	for (const auto& fd : processing_fds) {
		std::cout << "找到的fd= " << fd << " ";
	}
	ONLINEUSERIP oipinfo = {  };//上线用户的ip等信息
	HEAD_T head = {  };//协议头
	USERINFO_T user = {  };//协议体
	CHATMSG_T buf = {  };//协议体
	FEEDBACK_T back = {  };//反馈体
	USERRISTER risinfo = {  };//用户注册信息
	user = arg->userinfo;
	string username;
	username = user.user_name;

	cout << "用户:" << username << "已经和服务器断开连接" << endl;

	char* charPtr = const_cast<char*>(username.c_str());
	auto it = onlinemap.find(charPtr);
	for (const auto& pair : onlinemap) {
		std::cout << "Key: " << pair.first << ", Value: " << pair.second.oipstr << std::endl;
	}
	if (it != onlinemap.end()) {
		//向所有其他用户广播该用户下线
		FEEDBACK_T backlogsuccess = { 0 };//下线成功的反馈，向其他用户通知
		ONLINEUSERIP quituser;//初始化临时下线用户变量
		strcpy(quituser.ousername, username.c_str());
		cout << "quituser.ousername=" << quituser.ousername << endl;
		backlogsuccess.businessType = EXITLOG;
		backlogsuccess.flag = 1;
		backlogsuccess.businessLength = sizeof(ONLINEUSERIP);
		// 创建一个缓冲区
		char buffer[sizeof(FEEDBACK_T) + sizeof(ONLINEUSERIP)];
		memcpy(buffer, &backlogsuccess, sizeof(FEEDBACK_T));
		memcpy(buffer + sizeof(FEEDBACK_T), &quituser, sizeof(ONLINEUSERIP));
		for (auto it = onlinemap.begin(); it != onlinemap.end(); it++)
		{
			string suname = it->second.ousername;
			if (suname != username) {
				int sendfd = it->second.ofd;
				int port = it->second.oport;
				string ipstr = it->second.oipstr;
				/*cout << "sendfd =" << sendfd << endl;
				cout << "suname =" << suname << endl;
				cout << "ipstr =" << ipstr << endl;
				cout << "port = " << port << endl;*/
				cout << "收到用户 " << username << " 下线的消息，并转发给用户 " << suname << endl;
				write(sendfd, &buffer, sizeof(buffer));//发给接收者
				sleep(0.5);
			}
		}
		//上为实现向其他用户广播该用户下线		
			// 找到了匹配的键
		cout << "用户:" << username << "已经和服务器断开连接" << endl;
		std::cout << "Element found: " << it->first << ", " << it->second.oipstr << std::endl;
		//先移除根据套接字保存的用户数据再移除根据用户名保存的用户信息
		int tempfd = it->second.ofd;
		for (auto it2 = recorduser.begin(); it2 != recorduser.end(); it++)
		{
			if (it2->first == tempfd)
			{
				cout << "移除这个套接字的映射fd=" << it2->first << "该套接字用户名为:" << it2->second << endl;
				recorduser.erase(it2);
				break;
			}
		}
		onlinemap.erase(it);

	}
	else {
		// 没有找到匹配的键
		std::cout << "Element not found" << std::endl;

	}
	// 先从 epoll 中移除
	cout << "移除epoll并且关闭文件描述符" << endl;
	if (epoll_ctl(arg->epoll_fd, EPOLL_CTL_DEL, fd, NULL) == -1) {
		perror("epoll_ctl: del");
	}

	// 然后关闭文件描述符
	close(fd);
	cout << "当前在线人数为:" << onlinemap.size() << endl;
	cout << "结束了这次用户退出业务" << endl;
	processing_fds.erase(fd);
	mtx.unlock();
	return;
}


static string logpd(connectionpool* pool, int begin, int end, string username, string password)
{
	//从已创建好的队列取出一个可用的连接执行sql语句
	//cout << "logpd开始了" << endl;
	string result;//存储查询结果
	for (int i = begin; i < end; i++)
	{
		shared_ptr<mysqlclass> conn = pool->getconnection();
		//cout << "shareptr can go" << endl;
		char sql[1024] = { 0 };
		snprintf(sql, sizeof(sql), "select * from userinfo where username = '%s'", username.c_str());
		//cout << "sql=" << sql << endl;
		bool hasData = conn->query(sql); // 执行查询语句
		if (hasData) {
			// 获取查询结果的行数
			int rowCount = mysql_num_rows(conn->getresult());
			cout << "rowcount=" << rowCount << endl;
			if (rowCount > 0) {
				//cout << "数据库中存在你要查找的数据" << endl;
				//cout << "你输入的密码为:" << password << endl;
				if (conn->ispwdmtach(password))
				{
					//cout << "密码正确,执行下一步" << endl;
					conn->query(sql); // 再执行一次查询语句
					result = conn->getavatar();
					return result;
				}
				else
				{
					//cout << "密码错误!返回登录。" << endl;
					usernamepd = true;
					result = "false";
					return result;
				}
			}
			else {
				cout << "数据库中不存在你要查找的数据" << endl;
				usernamepd = false;
				result = "false";
				cout << "result=" << result << endl;
				return result;
			}
		}
		else {
			cout << "查询出现错误" << endl;
			return result;
		}
	}
	return result;

}

static bool regpd(connectionpool* pool, int begin, int end, string username, string password, string phone)
{
	//从已创建好的队列取出一个可用的连接执行sql语句
	cout << "regpd开始了" << endl;
	for (int i = begin; i < end; i++)
	{
		shared_ptr<mysqlclass> conn = pool->getconnection();
		//cout << "shareptr can go" << endl;
		char sql[1024] = { 0 };
		snprintf(sql, sizeof(sql), "select phone from userinfo where phone = '%s'", phone.c_str());
		cout << "sql=" << sql << endl;
		//sprintf(sql, "insert into student values(%d,0520663,'某个人c','男','2023-08-15')", i);
		bool hasData = conn->query(sql); // 执行查询语句
		if (hasData) {
			// 获取查询结果的行数
			int rowCount = mysql_num_rows(conn->getresult());
			if (rowCount > 0) {
				cout << "该电话号码已经注册账号" << endl;
				cout << "你输入的电话号码为:" << phone << endl;
				return true;
			}
			else
			{
				cout << "该电话号码还没有注册" << endl;
				return false;
			}
		}
		else {
			cout << "查找出错" << endl;
			return false;
		}


		return false;
	}
	return false;
}

static bool adduserinfotosql(connectionpool* pool, int begin, int end, string username, string password, string phone)
{
	//从已创建好的队列取出一个可用的连接执行sql语句
	cout << "adduserinfo开始了" << endl;
	for (int i = begin; i < end; i++)
	{
		shared_ptr<mysqlclass> conn = pool->getconnection();
		//cout << "shareptr can go" << endl;
		char sql[1024] = { 0 };
		snprintf(sql, sizeof(sql), "insert into userinfo(phone,password,username) values ('%s','%s','%s') ", phone.c_str(), password.c_str(), username.c_str());
		cout << "sql=" << sql << endl;
		//sprintf(sql, "insert into student values(%d,0520663,'某个人c','男','2023-08-15')", i);
		conn->updata(sql); // 执行更新数据库语句

		return true;
	}
	return false;
}

void falseuserquie(std::unique_ptr<ThreadArgs> arg)
{
	//处理当用户突然断开连接后，通知其他用户该用户下线的实现
	std::lock_guard<std::mutex> lock(mtx);
	cout << "开始了这次用户退出业务" << endl;
	int fd = *(arg->fd);//客户端连接服务器后的文件描述符
	processing_fds.insert(fd);//将其添加到正在处理的集合中
	ONLINEUSERIP oipinfo = {  };//上线用户的ip等信息
	HEAD_T head = {  };//协议头
	USERINFO_T user = {  };//协议体
	CHATMSG_T buf = {  };//协议体
	FEEDBACK_T back = {  };//反馈体
	USERRISTER risinfo = {  };//用户注册信息
	string findusername;
	for (auto it3 = recorduser.begin(); it3 != recorduser.end(); it3++)
	{
		if (it3->first == fd)
		{
			cout << "移除这个套接字的映射fd=" << it3->first << "该套接字用户名为:" << it3->second << endl;
			findusername = it3->second;
			break;
		}
	}

	string username;
	username = findusername;

	cout << "用户:" << username << "已经和服务器断开连接" << endl;

	char* charPtr = const_cast<char*>(username.c_str());
	auto it = onlinemap.find(charPtr);
	for (const auto& pair : onlinemap) {
		std::cout << "Key: " << pair.first << ", Value: " << pair.second.oipstr << std::endl;
	}
	if (it != onlinemap.end()) {
		//向所有其他用户广播该用户下线
		FEEDBACK_T backlogsuccess = { 0 };//下线成功的反馈，向其他用户通知
		ONLINEUSERIP quituser;//初始化临时下线用户变量
		strcpy(quituser.ousername, username.c_str());
		cout << "quituser.ousername=" << quituser.ousername << endl;
		backlogsuccess.businessType = EXITLOG;
		backlogsuccess.flag = 1;
		backlogsuccess.businessLength = sizeof(ONLINEUSERIP);
		// 创建一个缓冲区
		char buffer[sizeof(FEEDBACK_T) + sizeof(ONLINEUSERIP)];
		memcpy(buffer, &backlogsuccess, sizeof(FEEDBACK_T));
		memcpy(buffer + sizeof(FEEDBACK_T), &quituser, sizeof(ONLINEUSERIP));
		for (auto it = onlinemap.begin(); it != onlinemap.end(); it++)
		{
			string suname = it->second.ousername;
			if (suname != username) {
				int sendfd = it->second.ofd;
				int port = it->second.oport;
				string ipstr = it->second.oipstr;
				/*cout << "sendfd =" << sendfd << endl;
				cout << "suname =" << suname << endl;
				cout << "ipstr =" << ipstr << endl;
				cout << "port = " << port << endl;*/
				cout << "收到用户 " << username << " 下线的消息，并转发给用户 " << suname << endl;
				write(sendfd, &buffer, sizeof(buffer));//发给接收者
				sleep(0.5);
			}
		}
		//上为实现向其他用户广播该用户下线		
			// 找到了匹配的键
		cout << "用户:" << username << "已经和服务器断开连接" << endl;
		std::cout << "Element found: " << it->first << ", " << it->second.oipstr << std::endl;
		//先移除根据套接字保存的用户数据再移除根据用户名保存的用户信息
		int tempfd = it->second.ofd;
		for (auto it2 = recorduser.begin(); it2 != recorduser.end(); it++)
		{
			if (it2->first == tempfd)
			{
				cout << "移除这个套接字的映射fd=" << it2->first << "该套接字用户名为:" << it2->second << endl;
				recorduser.erase(it2);
				break;
			}
		}
		onlinemap.erase(it);

	}
	else {
		// 没有找到匹配的键
		std::cout << "Element not found" << std::endl;

	}
	// 先从 epoll 中移除
	cout << "移除epoll并且关闭文件描述符" << endl;
	if (epoll_ctl(arg->epoll_fd, EPOLL_CTL_DEL, fd, NULL) == -1) {
		perror("epoll_ctl: del");
	}

	// 然后关闭文件描述符
	close(fd);
	cout << "当前在线人数为:" << onlinemap.size() << endl;
	cout << "结束了这次用户退出业务" << endl;
	processing_fds.erase(fd);

	return;

}
/*// 其他的已连接 socket 有数据可读
				// 读取并处理数据，或者将其加入线程池进行处理
				int client_fd = events[i].data.fd;
				HEAD_T head;
				read(client_fd, &head, sizeof(HEAD_T));
				// 根据 head 处理
				if (head.businessType == LOGIN) {
					// 添加任务到线程池
					threadpool.enqueue([client_fd, pool, head, epoll_fd] {
						// TODO: 在这里添加处理登录业务的代码
						std::unique_ptr<ThreadArgs> arg(new ThreadArgs());
						arg->fd = std::unique_ptr<int>(new int(client_fd));
						arg->pool = pool;
						arg->epoll_fd = epoll_fd;
						arg->infolength = head.businessLength;
						cout << "head.businessLength=" << arg->infolength << endl;
						handlelog(std::move(arg));
						});
				}*/

