#include<stdio.h>
#include<winsock2.h>
#include<iostream>
#include <iomanip> // 包含 std::setw, std::left, std::right, std::internal
#include <windows.h>
#pragma comment(lib,"ws2_32.lib")
using namespace std;

# define BUF_SIZE 150

void setConsoleColor(int color) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, color);
}

DWORD WINAPI USend(LPVOID sockpara) 
{
    SOCKET * sock = (SOCKET*)sockpara;
    char bufSend[BUF_SIZE] = { 0 };
    while (1) {
        //printf("Input a string: ");
        std::cin >> bufSend;
        cout << "------------------------------------" << std::endl;
        int t = send(*sock, bufSend, strlen(bufSend), 0);
        SYSTEMTIME st = { 0 };
        GetLocalTime(&st);
        if (strcmp(bufSend, "exit()") == 0){
            closesocket(*sock);
            std::cout << "you " <<st.wMonth<<"."<< st.wDay << "  " << st.wHour << ":" << st.wMinute << ":" << st.wSecond
            << "s exit." << std::endl;
            return 0;
        }
        if (t > 0) {
            cout << "message has sended at " << st.wMonth<<"."<<st.wDay << "  " << st.wHour << ":" << st.wMinute << ":" << st.
            wSecond << "s \n" ;
            cout << "--------------------------------" << std::endl;
        }
        memset(bufSend, 0, BUF_SIZE);
    }
}

DWORD WINAPI URecv(LPVOID sock_) {
    char bufRecv[BUF_SIZE] = { 0 };
    SOCKET *sock = (SOCKET*)sock_;
    char name[50]={' '};//用于表示用户，由Sever分配，用#分割
    char color=' ';//用于表示不同用户的不同字色；用#分割
    char judgechar[7];
    while (1) {
        int t = recv(*sock, bufRecv, BUF_SIZE, 0);
        strncpy(judgechar,bufRecv,6);
        judgechar[6]='\0';
        SYSTEMTIME st = { 0 };
        GetLocalTime(&st);
        if (strcmp(judgechar, "exit()") == 0){//exit()name
            strncpy(name,bufRecv+6,sizeof(bufRecv)-6);
            // closesocket(*sock);
            setConsoleColor(12); // 设置文本为红色
            std::cout <<"user "<<name <<" exit at "<< st.wMonth << "." <<st.wDay << "  " << st.wHour << ":" << st.wMinute << ":" << st.
            wSecond << "s." << std::endl;
            setConsoleColor(7); // 设置文本为白色
        }
        else{
             // 手动查找 '#' 并在找到的位置添加 '\0'
            char* userpos = strchr(bufRecv, '#'); // 查找字符 '#'
            if (userpos != nullptr) {
                strncpy(name,bufRecv,userpos-bufRecv);
                name[userpos-bufRecv]='\0';
            }
            if(userpos!=nullptr){         
                char* colorpos = strchr(userpos+1, '#'); // 查找字符 '#'
                if (colorpos != nullptr) {
                    color=*(colorpos-1);
                    // strncpy(bufRecv,(colorpos+1),strlen(bufRecv));
                    strncpy(bufRecv, colorpos + 1, BUF_SIZE - (colorpos + 1 - bufRecv));
                    bufRecv[BUF_SIZE - 1] = '\0'; // 确保消息以 '\0' 结尾
                    // *colorpos = '\0'; // 在 '#' 处添加结束符
                }
            }
            else{
                // cout<<"username not found."<<endl;
            }
        }
        if (t > 0) 
        {
            if(strlen(bufRecv)!=0)
            {
                if(color=='b')setConsoleColor(1); // 设置文本颜色，1为蓝
                else if(color=='g')setConsoleColor(2);
                else if(color=='y')setConsoleColor(6);
                else if(color=='p')setConsoleColor(5);        
                std::cout <<  st.wMonth << "." <<st.wDay << "  " << st.wHour << ":" << st.wMinute << ":" << st.wSecond <<  std::setw(5) << std::left <<"s"<<name<<":"<<endl;
                printf(" %s\n", bufRecv);
                setConsoleColor(7);
                std::cout << "-------------------------------" << std::endl;
            }
            else;//什么也没发
        }
        memset(bufRecv, 0, BUF_SIZE);
    }
 }
void StartClient() {
    WSADATA wsadata;
    WSAStartup(MAKEWORD(2, 2), &wsadata);

    // 创建套接字
    SOCKET clientSock = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in socketAddress;
    memset(&socketAddress, 0, sizeof(socketAddress));
    socketAddress.sin_family = AF_INET;        
    socketAddress.sin_addr.s_addr = inet_addr("127.0.0.1");       
    socketAddress.sin_port = htons(426); // 连接到端口 

    if (connect(clientSock, (SOCKADDR*)&socketAddress, sizeof(socketAddress)) == 0) {
        cout << "Connected to server successfully!" << endl;
        // 创建收发消息的线程
        HANDLE hthread[2];
        hthread[0] = CreateThread(NULL, 0, URecv, (LPVOID)&clientSock, 0, NULL);
        hthread[1] = CreateThread(NULL, 0, USend, (LPVOID)&clientSock, 0, NULL);
        WaitForMultipleObjects(2, hthread, FALSE, INFINITE);
        CloseHandle(hthread[0]);
        CloseHandle(hthread[1]);
        closesocket(clientSock);
        WSACleanup();

    } else {
        cout << "Connection failed: " << WSAGetLastError() << endl; // 添加错误处理
    }
}

int main(){
    StartClient();
}
