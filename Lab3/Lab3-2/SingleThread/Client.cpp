#include <iostream>
#include <WINSOCK2.h>
#include <time.h>
#include <fstream>
#include <iomanip>
#pragma comment(lib, "ws2_32.lib")
using namespace std;

const int MAXSIZE = 1024;//传输缓冲区最大长度
const unsigned char SYN = 0x1; //SYN = 1 ACK = 0
const unsigned char ACK = 0x2;//SYN = 0, ACK = 1，FIN = 0
const unsigned char ACK_SYN = 0x3;//SYN = 1, ACK = 1
const unsigned char FIN = 0x4;//FIN = 1 ACK = 0
const unsigned char FIN_ACK = 0x5;
const unsigned char OVER = 0x7;//结束标志

const int max_windows_size = 4;//最大窗口大小

double MAX_TIME = 0.5 * CLOCKS_PER_SEC;


void setColor(int color_code) {
    std::cout << "\033[" << color_code << "m";
}

void resetColor() {
    std::cout << "\033[0m";
}

/*
1.把伪首部添加到UDP上；
2.计算初始时是需要将检验和字段添零的；
3.把所有位划分为16位（2字节）的字
4.把所有16位的字相加，如果遇到进位，则将高于16字节的进位部分的值加到最低位上，举例，0xBB5E+0xFCED=0x1 B84B，则将1放到最低位，得到结果是0xB84C
5.将所有字相加得到的结果应该为一个16位的数，将该数取反则可以得到检验和checksum。 */
u_short cksum(u_short* mes, int size) {
    int count = (size + 1) / 2;
    u_short* buf = (u_short*)malloc(size + 1);
    memset(buf, 0, size + 1);
    memcpy(buf, mes, size);
    u_long sum = 0;
    while (count--) {
        sum += *buf++;
        if (sum & 0xffff0000) {
            sum &= 0xffff;
            sum++;
        }
    }
    return ~(sum & 0xffff);
}

struct HEADER
{
    u_short sum = 0;//校验和 16位
    u_short datasize = 0;//所包含数据长度 16位
    unsigned char flag = 0;
    //八位，使用后四位，排列是FIN ACK SYN 
    unsigned char SEQ = 0;
    //八位，传输的序列号，0~255，超过后mod
    HEADER() {
        sum = 0;//校验和 16位
        datasize = 0;//所包含数据长度 16位
        flag = 0;
        //八位，使用后三位，排列是FIN ACK SYN 
        SEQ = 0;
    }
};

int Connect(SOCKET& socketClient, SOCKADDR_IN &servAddr, int& servAddrlen)//三次握手建立连接
{
    HEADER header;
    char* Buffer = new char[sizeof(header)];

    u_short sum;

    //进行第一次握手
    header.flag = SYN;
    header.sum = 0;//校验和置0
    u_short temp = cksum((u_short*)&header, sizeof(header));
    header.sum = temp;//计算校验和
    memcpy(Buffer, &header, sizeof(header));//将首部放入缓冲区
    if (sendto(socketClient, Buffer, sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen) == -1)
    {
        return -1;
    }
    clock_t start = clock(); //记录发送第一次握手时间

    u_long mode = 1;
    ioctlsocket(socketClient, FIONBIO, &mode);

    int checkcount=0;
    //接收第二次握手
    while (recvfrom(socketClient, Buffer, sizeof(header), 0, (sockaddr*)&servAddr, &servAddrlen) <= 0)
    {
        if (clock() - start > MAX_TIME)//超时，重新传输第一次握手
        {
            if(checkcount<=10)
            {
                header.flag = SYN;
                header.sum = 0;//校验和置0
                header.sum = cksum((u_short*)&header, sizeof(header));//计算校验和
                memcpy(Buffer, &header, sizeof(header));//将首部放入缓冲区
                sendto(socketClient, Buffer, sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen);
                start = clock();
                std::cout << "第一次握手超时，正在进行重传" << endl;
                checkcount++;
            }
            else{
                std::cout<<"重传次数过多，程序退出"<<std::endl;
                return -1;
            }
        }
    }

    
    //进行校验和检验
    memcpy(&header, Buffer, sizeof(header));
    if (header.flag == ACK && cksum((u_short*)&header, sizeof(header) == 0))
    {
        cout << "收到第二次握手信息" << endl;
    }
    else
    {
        cout << "连接发生错误，请重启客户端！" << endl;
        return - 1;
    }

    //进行第三次握手
    header.flag = ACK_SYN;
    header.sum = 0;
    header.sum = cksum((u_short*)&header, sizeof(header));//计算校验和
    if (sendto(socketClient, (char*) & header, sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen) == -1)
    {
        return -1;//判断客户端是否打开，-1为未开启发送失败
    }
    cout << "服务器成功连接！可以发送数据" << endl;
    return 1;
}

//将整个缓冲区内的数据再发一遍
void resend_windows( SOCKET& socketClient, SOCKADDR_IN& servAddr, int& servAddrlen,HEADER header,char* buffer,char* message,int len,int windowspos)
{
    header.datasize = len;
    // header.SEQ = u_char(windowspos)+1>255?0:u_char(windowspos)+1;//序列号
    header.SEQ = u_char(windowspos);//序列号
    header.flag = u_char(0x0);
    memcpy(buffer, &header, sizeof(header));
   // memcpy(buffer + sizeof(header), message-MAXSIZE*(max_windows_size-1), len);
    memcpy(buffer + sizeof(header), message, len);
    u_short check = cksum((u_short*)buffer, sizeof(header) + len);//计算校验和
    header.sum = check;
    memcpy(buffer, &header, sizeof(header));
    sendto(socketClient, buffer, len + sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen);//发送
    cout << "TIME OUT! ReSend message " << len << " bytes! Flag:" << int(header.flag) << " SEQ:" << int(header.SEQ) << endl;

}

void send_package(SOCKET& socketClient, SOCKADDR_IN& servAddr, int& servAddrlen, char* message, int len, int &order,int& windowspos)
{
    HEADER header;
    char* buffer = new char[MAXSIZE + sizeof(header)];
    header.datasize = len;
    header.SEQ = u_char(order);//序列号
    memcpy(buffer, &header, sizeof(header));
    memcpy(buffer + sizeof(header), message, len);
    u_short check = cksum((u_short *) buffer, sizeof(header) + len);//计算校验和
    header.sum = check;
    memcpy(buffer, &header, sizeof(header));
    clock_t start = clock();//记录发送时间
    if(((unsigned int) header.SEQ-windowspos<= max_windows_size)||
        (header.SEQ-windowspos<0 && header.SEQ+255-windowspos<=max_windows_size) )
    {
        sendto(socketClient, buffer, len + sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen);//发送
        setColor(32);
        // 输出文本行
        std::cout << "Send windows: ["<<windowspos-4<<"……"<<windowspos+max_windows_size-4<<"]" << std::endl;            
        // 假设从第18列开始
        int startColumn = 20;
        // 输出换行符和空格以对齐"^"
        std::cout << std::setw(startColumn) << '^' << std::endl;
        resetColor();
        std::cout << "Send message " << len << " bytes!" << " flag:" << int(header.flag) << " SEQ:" << int(header.SEQ) << " SUM:" << int(header.sum) << std::endl;
        start = clock();//update发送时间
    }
    else{
        setColor(33);
        std::cout<<"Windows is full.Please waiting."<<endl;
        resetColor();
    }
    //接收ack等信息
    while (1 == 1)
    {
        u_long mode = 1;
        ioctlsocket(socketClient, FIONBIO, &mode);
        //判断是否接收到包，主要针对发送端或服务器端丢包的问题。
        while (recvfrom(socketClient, buffer, MAXSIZE, 0, (sockaddr*)&servAddr, &servAddrlen) <= 0)
        {
            if (clock() - start > MAX_TIME)
            {
                //这里其实大部分时候只会把最前面的包丢了，并且丢包后接受的ack不会让该函数完成
                resend_windows(socketClient,servAddr,servAddrlen,header,buffer,message,len,windowspos);
                start = clock();//记录发送时间
                
            }
        }
        memcpy(&header, buffer, sizeof(header));//缓冲区接收到信息，读取
        u_short check = cksum((u_short*)&header, sizeof(header));
        //判断接受的包的序列号是否正确；这里有两种特殊情况。
        //一是接受的SEQ为0，1…… 但是窗口位置在254，255……；这种时候应该能够正常发送并等待前面部分重发
        //二是接受的      
        if(((header.SEQ-windowspos>=0 && (header.SEQ-windowspos)<= max_windows_size)||
           ( header.SEQ-windowspos <0 && (header.SEQ+255-windowspos)<=max_windows_size) ) && header.flag==ACK)
        {
                      
            cout << "Send has been confirmed! Flag:" << int(header.flag)<< " SEQ:" << int(header.SEQ) << endl;
            resetColor();
            if(header.SEQ==windowspos)
            {
                windowspos++;
                start=clock();
            }
            if(windowspos>255)windowspos-=256;
            
            break;
        }
        //接收到的ACK的SEQ不在窗口范围内，丢弃
        else
        {
            setColor(33);
            // 输出文本行
            std::cout << "Current windows: ["<<windowspos<<"……"<<windowspos+max_windows_size<<"]" << std::endl;            
            resetColor();
            cout << "Receive plain Flag:" << int(header.flag)<< " SEQ:" << int(header.SEQ) << endl;
            break;
        }
    }
    u_long mode = 0;
    ioctlsocket(socketClient, FIONBIO, &mode);//改回阻塞模式
}

void send(SOCKET& socketClient, SOCKADDR_IN& servAddr, int& servAddrlen, char* message, long long int len)
{
    int packagenum = len / MAXSIZE + (len % MAXSIZE != 0);
    int seqnum = 0;
    int basepos = 0;
    for (int i = 0; i < packagenum; i++)
    {
        
        send_package(socketClient, servAddr, servAddrlen, message + i * MAXSIZE, i == packagenum - 1? len - (packagenum - 1) * MAXSIZE : MAXSIZE, seqnum,basepos);
        seqnum = (seqnum + 1) % 256; // 确保序列号在 0-255 循环
        
    }
    //发送结束信息
end:
    HEADER header;
    char* Buffer = new char[sizeof(header)];
    header.flag = OVER;
    header.sum = 0;
    u_short temp = cksum((u_short*)&header, sizeof(header));
    header.sum = temp;
    memcpy(Buffer, &header, sizeof(header));
    sendto(socketClient, Buffer, sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen);
    cout << "Send End!" << endl;
    clock_t start = clock();
    while (1 == 1)
    {
        u_long mode = 1;
        ioctlsocket(socketClient, FIONBIO, &mode);
        while (recvfrom(socketClient, Buffer, MAXSIZE, 0, (sockaddr*)&servAddr, &servAddrlen) <= 0)
        {
            if (clock() - start > MAX_TIME)
            {
                char* Buffer = new char[sizeof(header)];
                header.flag = OVER;
                header.sum = 0;
                u_short temp = cksum((u_short*)&header, sizeof(header));
                header.sum = temp;
                memcpy(Buffer, &header, sizeof(header));
                sendto(socketClient, Buffer, sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen);
                cout << "Time Out! ReSend End!" << endl;
                start = clock();
            }
        }
        memcpy(&header, Buffer, sizeof(header));//缓冲区接收到信息，读取
        u_short check = cksum((u_short*)&header, sizeof(header));
        if (header.flag == OVER)
        {
            setColor(31);
            cout << "对方已成功接收文件!" << endl;
            resetColor();
            break;
        }
        else
        {
            continue;
        }
    }
    u_long mode = 0;
    ioctlsocket(socketClient, FIONBIO, &mode);//改回阻塞模式
}



int disConnect(SOCKET& socketClient, SOCKADDR_IN& servAddr, int& servAddrlen)
{
    HEADER header;
    char* Buffer = new char[sizeof(header)];

    u_short sum;

    //进行第一次握手
    header.flag = FIN;
    header.sum = 0;//校验和置0
    u_short temp = cksum((u_short*)&header, sizeof(header));
    header.sum = temp;//计算校验和
    memcpy(Buffer, &header, sizeof(header));//将首部放入缓冲区
    if (sendto(socketClient, Buffer, sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen) == -1)
    {
        return -1;
    }
    clock_t start = clock(); //记录发送第一次挥手时间

    u_long mode = 1;
    ioctlsocket(socketClient, FIONBIO, &mode);

    //接收第二次挥手
    while (recvfrom(socketClient, Buffer, sizeof(header), 0, (sockaddr*)&servAddr, &servAddrlen) <= 0)
    {
        if (clock() - start > MAX_TIME)//超时，重新传输第一次挥手
        {
            header.flag = FIN;
            header.sum = 0;//校验和置0
            header.sum = cksum((u_short*)&header, sizeof(header));//计算校验和
            memcpy(Buffer, &header, sizeof(header));//将首部放入缓冲区
            sendto(socketClient, Buffer, sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen);
            start = clock();
            cout << "第一次挥手超时，正在进行重传" << endl;
        }
    }


    //进行校验和检验
    memcpy(&header, Buffer, sizeof(header));
    if (header.flag == ACK && cksum((u_short*)&header, sizeof(header) == 0))
    {
        cout << "收到第二次挥手信息" << endl;
    }
    else
    {
        cout << "连接发生错误，程序直接退出！" << endl;
        return -1;
    }

    //进行第三次挥手
    header.flag = FIN_ACK;
    header.sum = 0;
    header.sum = cksum((u_short*)&header, sizeof(header));//计算校验和
    if (sendto(socketClient, (char*)&header, sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen) == -1)
    {
        return -1;
    }

    start = clock();
    //接收第四次挥手
    while (recvfrom(socketClient, Buffer, sizeof(header), 0, (sockaddr*)&servAddr, &servAddrlen) <= 0)
    {
        if (clock() - start > MAX_TIME)//超时，重新传输第三次挥手
        {
            header.flag = FIN;
            header.sum = 0;//校验和置0
            header.sum = cksum((u_short*)&header, sizeof(header));//计算校验和
            memcpy(Buffer, &header, sizeof(header));//将首部放入缓冲区
            sendto(socketClient, Buffer, sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen);
            start = clock();
            cout << "第四次握手超时，正在进行重传" << endl;
        }
    }
    cout << "四次挥手结束，连接断开！" << endl;
    return 1;
}


int main()
{
    WSADATA wsadata;
    WSAStartup(MAKEWORD(2, 2), &wsadata);

    SOCKADDR_IN server_addr;
    SOCKET server;

    server_addr.sin_family = AF_INET;//使用IPV4
    server_addr.sin_port = htons(2456);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    server = socket(AF_INET, SOCK_DGRAM, 0);
    int len = sizeof(server_addr);
    //建立连接
    if (Connect(server, server_addr, len) == -1)
    {
        return 0;
    }
    string Command;
    while(1){
        cout << "请输入命令" << endl;
        cin >> Command;
        if (Command == "exit")
        {
            char blank[10];
            send(server, server_addr, len, blank, int(0));
            disConnect(server, server_addr, len);
            break;
        }
        string filename;
        cout << "请输入文件名称" << endl;
        cin >> filename;
        ifstream fin(filename.c_str(), ifstream::binary);//以二进制方式打开文件
        char* buffer = new char[20000000];
        long long int index = 0;
        unsigned char temp = fin.get();
        while (fin)
        {
            buffer[index++] = temp;
            temp = fin.get();
        }
        fin.close();
        send(server, server_addr, len, (char*)(filename.c_str()), filename.length());
        clock_t start = clock();
        cout<<"发送文件大小为:"<<index<<endl;
        send(server, server_addr, len, buffer, index);
        clock_t end = clock();
        delete buffer;
        cout << "传输总时间为:" << (end - start-200) / CLOCKS_PER_SEC << "s" << endl;
        cout << "吞吐率为:" << ((float)index ) / ((end - start-200) /CLOCKS_PER_SEC) << "byte/s" << endl;
    }
    system("pause");
}
