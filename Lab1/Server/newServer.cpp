#include <stdio.h>
#include <winsock2.h>
#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <pthread.h>
#include <mutex>
#include <algorithm>//包含迭代器的库
#include <future>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

#define BUF_SIZE 150

// 用于存储当前连接的客户端套接字
vector<SOCKET> clients;

mutex mtx; // 保护消息的互斥量
//char ms[]

// 广播消息到所有客户端；最终格式为"username#color#message"
void broadcastMessage(const char* message, SOCKET senderSock,char*c1,char c2) {
    //lock_guard<mutex> lock(mtx); // 保护共享资源
    for (SOCKET sock : clients) {
        if (sock != senderSock) { // 不发送给发送者
            char newms[150];
            if(strcmp(message,"exit()")==0){
                strncpy(newms,message,6);
                strncpy(newms+6,c1,strlen(c1));
                newms[6+strlen(c1)]='\0';
            }
            else{
                strncpy(newms,c1,strlen(c1));
                newms[strlen(c1)]='#';
                newms[strlen(c1)+1]=c2;
                newms[strlen(c1)+2]='#';
                strncpy(newms+strlen(c1)+3,message,strlen(message));
                newms[(strlen(c1)+3+strlen(message))]='\0';
            }
            send(sock, newms, strlen(newms), 0);
        }
    }
}

// 处理每个客户端的线程函数
DWORD WINAPI handleClient(LPVOID param) {
    SOCKET clientSock = *(SOCKET*)param;
    char bufRecv[BUF_SIZE] = { 0 };
    char username[50]={' '};
    char usercolor=' ';
    cout<<"input name and color:";
    cin>>username>>usercolor;
    char judgechar[7]={' '};
    char message[102]={' '};
    while (true) {
        int bytesReceived = recv(clientSock, bufRecv, BUF_SIZE, 0);
        strncpy(judgechar,bufRecv,6);
        judgechar[6]='\0';
        if (bytesReceived <= 0) {
            break; // 连接关闭或出错
        }
        if (strcmp(judgechar, "exit()") == 0){  // 有用户退出
            memcpy(message,"exit()",6);
            broadcastMessage(message,clientSock,username,usercolor);
            break;
        }
        // 广播接收到的消息
        broadcastMessage(bufRecv, clientSock,username,usercolor);
        memset(bufRecv, 0, BUF_SIZE); // 清空接收缓冲区
    }

    // 客户端断开连接，移除其套接字
    closesocket(clientSock);
    {
        //lock_guard<mutex> lock(mtx); // 保护共享资源
        // 找到 clientSock 在 clients 向量中的位置
        auto it =find(clients.begin(), clients.end(), (clientSock));
        // 如果找到了，删除它
        if (it != clients.end()) {
            clients.erase(it);
            std::cout << "client "<<clientSock<<" withdraw." << std::endl;
        } else {
            std::cout << "has been deleted ~ " << std::endl;
        }
    }
    return 0;
}

void StartServer() {
    WSADATA wsadata;
    WSAStartup(MAKEWORD(2, 2), &wsadata);

    sockaddr_in socketAddress;
    memset(&socketAddress, 0, sizeof(socketAddress));
    socketAddress.sin_family = AF_INET;
    socketAddress.sin_addr.s_addr = inet_addr("127.0.0.1"); 

    // 监听第一个端口 426
    socketAddress.sin_port = htons(426);
    SOCKET listenSock1 = socket(AF_INET, SOCK_STREAM, 0);
    if (bind(listenSock1, (SOCKADDR*)&socketAddress, sizeof(socketAddress)) == SOCKET_ERROR) {
        cout << "Bind failed on port 426: " << WSAGetLastError() << endl;
        return;
    }
    listen(listenSock1, SOMAXCONN);
    cout << "The server is waiting for connection on port 426..." << endl;

    // 监听第二个端口 425
    socketAddress.sin_port = htons(425);
    SOCKET listenSock2 = socket(AF_INET, SOCK_STREAM, 0);
    if (bind(listenSock2, (SOCKADDR*)&socketAddress, sizeof(socketAddress)) == SOCKET_ERROR) {
        cout << "Bind failed on port 425: " << WSAGetLastError() << endl;
        closesocket(listenSock1);
        WSACleanup();
        return;
    }
    listen(listenSock2, SOMAXCONN);
    cout << "The server is waiting for connection on port 425..." << endl;

    // 监听第三个端口 424
    socketAddress.sin_port = htons(424);
    SOCKET listenSock3 = socket(AF_INET, SOCK_STREAM, 0);
    if (bind(listenSock3, (SOCKADDR*)&socketAddress, sizeof(socketAddress)) == SOCKET_ERROR) {
        cout << "Bind failed on port 424: " << WSAGetLastError() << endl;
        return;
    }
    listen(listenSock3, SOMAXCONN);
    cout << "The server is waiting for connection on port 424..." << endl;

    /*
    这里重点介绍一下文件描述符集合（fd_set）。
    它是一个存储文件描述符（每个打开的文件、网络连接等，系统会返回一个整数，称为文件描述符）的结构，通常用于监视一组文件描述符的状态
    通过select() 函数检查 fd_set 中的每个文件描述符，确定哪些文件描述符已经准备好进行 I/O 操作。
    */
    fd_set readfds;
    while (true) {
        // 每次循环都重置 fd_set
        FD_ZERO(&readfds);
        FD_SET(listenSock1, &readfds);
        FD_SET(listenSock2, &readfds);
        FD_SET(listenSock3, &readfds);

        // 设置超时为 NULL，表示阻塞
        timeval timeout;
        timeout.tv_sec = 1; // 1秒超时
        timeout.tv_usec = 0;

        // 等待连接
        int activity = select(0, &readfds, NULL, NULL, &timeout);
        if (activity < 0) {
            cout << "Select error: " << WSAGetLastError() << endl;
            break; // 出现错误，退出循环
        }

        // 检查第一个套接字
        if (FD_ISSET(listenSock1, &readfds)) {
            SOCKET clientSock = accept(listenSock1, NULL, NULL);
            if (clientSock != INVALID_SOCKET) {
                clients.push_back(clientSock);
                cout << "New client has connected on port 426: " << clientSock << endl;

                // 为新客户端创建线程
                CreateThread(NULL, 0, handleClient, (LPVOID)&clientSock, 0, NULL);
            } else {
                cout << "Accept failed on port 426: " << WSAGetLastError() << endl;
            }
        }

        // 检查第二个套接字
        if (FD_ISSET(listenSock2, &readfds)) {
            SOCKET clientSock = accept(listenSock2, NULL, NULL);
            if (clientSock != INVALID_SOCKET) {
                clients.push_back(clientSock);
                cout << "New client has connected on port 425: " << clientSock << endl;

                // 为新客户端创建线程
                CreateThread(NULL, 0, handleClient, (LPVOID)&clientSock, 0, NULL);
            } else {
                cout << "Accept failed on port 425: " << WSAGetLastError() << endl;
            }
        }
        
         // 检查第三个套接字
        if (FD_ISSET(listenSock3, &readfds)) {
            SOCKET clientSock = accept(listenSock3, NULL, NULL);
            if (clientSock != INVALID_SOCKET) {
                clients.push_back(clientSock);
                cout << "New client has connected on port 424: " << clientSock << endl;

                // 为新客户端创建线程
                CreateThread(NULL, 0, handleClient, (LPVOID)&clientSock, 0, NULL);
            } else {
                cout << "Accept failed on port 424: " << WSAGetLastError() << endl;
            }
        }
     
    }
    // 清理
    WSACleanup();
}

int main(){
    StartServer();
}