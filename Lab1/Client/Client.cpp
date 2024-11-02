#include<stdio.h>
#include<WinSock2.h>
#include<iostream>
#pragma comment(lib,"ws2_32.lib")
using namespace std;
# define BUF_SIZE 100
DWORD WINAPI Send(LPVOID sockpara) 
{
    SOCKET * sock = (SOCKET*)sockpara;
    char bufSend[BUF_SIZE] = { 0 };
    while (1) {
        //printf("Input a string: ");
        std::cin >> bufSend;
        int t = send(*sock, bufSend, strlen(bufSend), 0);
        SYSTEMTIME st = { 0 };
        GetLocalTime(&st);
        if (strcmp(bufSend, "exit()") == 0){
            closesocket(*sock);
            std::cout << "您已于" << st.wDay << "日" << st.wHour << "时" << st.wMinute << "分" << st.wSecond
            << "秒退出聊天室" << std::endl;
            return 0;
        }
        if (t > 0) {
            cout << "消息已于" << st.wDay << "日" << st.wHour << "时" << st.wMinute << "分" << st.
            wSecond << "秒成功发送\n" ;
            cout << "-------------------------------------------------------------" << std::endl;
        }
    memset(bufSend, 0, BUF_SIZE);
    }
}

DWORD WINAPI Recv(LPVOID sock_) {
    char bufRecv[BUF_SIZE] = { 0 };
    SOCKET *sock = (SOCKET*)sock_;
    while (1) {
        int t = recv(*sock, bufRecv, BUF_SIZE, 0);
        if (strcmp(bufRecv, "exit()") == 0){
            SYSTEMTIME st = { 0 };
            GetLocalTime(&st);
            closesocket(*sock);
            std::cout << "对方已于" << st.wDay << "日" << st.wHour << "时" << st.wMinute << "分" << st.
            wSecond << "秒下线退出聊天室" << std::endl;
            return 0L;
            }
        if (t > 0) {
            SYSTEMTIME st = { 0 };
            GetLocalTime(&st);
            std::cout << st.wDay << "日" << st.wHour << "时" << st.wMinute << "分" << st.wSecond << "秒收到消息:";
            printf(" %s\n", bufRecv);
            std::cout << "-------------------------------------------------------------" << std::endl;
        }
    memset(bufRecv, 0, BUF_SIZE);
 }
 }
void StartClient(){
WSADATA wsadata;
/*
连接到服务器
*/
sockaddr_in socketAddress;
 memset(&socketAddress, 0, sizeof(socketAddress));
    
    // 设置地址族为 IPv4
    socketAddress.sin_family = AF_INET;
    
    // 设置 IP 地址为 127.0.0.1
    socketAddress.sin_addr.s_addr = inet_addr("127.0.0.1");
    
    // 设置端口号为 425，使用 htons 转换为网络字节序
    socketAddress.sin_port = htons(425);

    // 创建套接字
SOCKET sock=socket(AF_INET,SOCK_STREAM,0);
if(connect(sock,(SOCKADDR*)&socketAddress,sizeof(socketAddress))==0){
    cout<<"Success"<<endl;
}
else{
    cout<<"服务器未上线~"<<endl;
}
/*
创建收发消息的端口
*/
HANDLE hthread[2];
hthread[0]=CreateThread(NULL,0,Recv,(LPVOID)&sock,0,NULL);
hthread[0]=CreateThread(NULL,0,Send,(LPVOID)&sock,0,NULL);
WaitForMultipleObjects(2,hthread,TRUE,INFINITE);
CloseHandle(hthread[0]);
CloseHandle(hthread[1]);
closesocket(sock);
WSACleanup();
}
