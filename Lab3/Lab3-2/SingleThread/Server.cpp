#include <iostream>
#include <WINSOCK2.h>
#include <iomanip>
#include <time.h>
#include <fstream>
#pragma comment(lib, "ws2_32.lib")
using namespace std;


const int MAXSIZE = 1024;//传输缓冲区最大长度
const unsigned char SYN = 0x1; //SYN = 1 ACK = 0
const unsigned char ACK = 0x2;//SYN = 0, ACK = 1
const unsigned char ACK_SYN = 0x3;//SYN = 1, ACK = 1
const unsigned char FIN = 0x4;//FIN = 1 ACK = 0
const unsigned char FIN_ACK = 0x5;//FIN = 1 ACK = 0
const unsigned char OVER = 0x7;//结束标志

int max_windows_size=4;

double MAX_TIME = 0.5 * CLOCKS_PER_SEC;

void setColor(int color_code) {
    std::cout << "\033[" << color_code << "m";
}

void resetColor() {
    std::cout << "\033[0m";
}


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
    //八位，使用后三位，排列是FIN ACK SYN 
    unsigned char SEQ = 0;
    //八位，传输的序列号，0~255，超过后mod
    HEADER() {
        sum = 0;//校验和 16位
        datasize = 0;//所包含数据长度 16位
        flag = 0;
        //八位，使用后四位，排列是FIN ACK SYN 
        SEQ = 0;
    }
};

int Connect(SOCKET& sockServ, SOCKADDR_IN& ClientAddr, int& ClientAddrLen)
{
    
    HEADER header;
    char* Buffer = new char[sizeof(header)];

    //接收第一次握手信息
    while (1 == 1)
    {
        if (recvfrom(sockServ, Buffer, sizeof(header), 0, (sockaddr*)&ClientAddr, &ClientAddrLen) == -1) 
        {
            return -1;
        }
        memcpy(&header, Buffer, sizeof(header));
        if (header.flag == SYN && cksum((u_short*)&header, sizeof(header)) == 0)
        {
            cout << "成功接收第一次握手信息" << endl;
            break;
        }
    }

    //发送第二次握手信息
    header.flag = ACK;
    header.sum = 0;
    u_short temp = cksum((u_short*)&header, sizeof(header));
    header.sum = temp;
    memcpy(Buffer, &header, sizeof(header));
    if (sendto(sockServ, Buffer, sizeof(header), 0, (sockaddr*)&ClientAddr, ClientAddrLen) == -1)
    {
        return -1;
    }
    clock_t start = clock();//记录第二次握手发送时间

    //接收第三次握手
    while (recvfrom(sockServ, Buffer, sizeof(header), 0, (sockaddr*)&ClientAddr, &ClientAddrLen) <= 0)
    {
        if (clock() - start > MAX_TIME)
        {
            header.flag = ACK;
            header.sum = 0;
            u_short temp = cksum((u_short*)&header, sizeof(header));
            header.flag = temp;
            memcpy(Buffer, &header, sizeof(header));
            if (sendto(sockServ, Buffer, sizeof(header), 0, (sockaddr*)&ClientAddr, ClientAddrLen) == -1)
            {
                return -1;
            }
            cout << "第二次握手超时，正在进行重传" << endl;
        }
    }

    HEADER temp1;
    memcpy(&temp1, Buffer, sizeof(header));
    if (temp1.flag == ACK_SYN && cksum((u_short*)&temp1, sizeof(temp1) == 0))
    {
        cout << "成功建立通信！可以接收数据" << endl;
    }
    else
    {
        cout << "serve连接发生错误，请重启客户端！" << endl;
        return -1;
    }
    return 1;
}

int RecvMessage(SOCKET& sockServ, SOCKADDR_IN& ClientAddr, int& ClientAddrLen, char *message)
{
    long int all = 0;//文件长度
    HEADER header;
    char* Buffer = new char[MAXSIZE + sizeof(header)];
    int seq = 0;
    int index = 0;
    int windowspos=0;
    while (1 == 1)
    {
        int length = recvfrom(sockServ, Buffer, sizeof(header) + MAXSIZE, 0, (sockaddr*)&ClientAddr, &ClientAddrLen);//接收报文长度
        //cout << length << endl;
        memcpy(&header, Buffer, sizeof(header));
        //判断是否是结束
        if (header.flag == OVER && cksum((u_short*)&header, sizeof(header)) == 0)
        {
            setColor(31);
            std::cout << "文件接收完毕" << endl;
            resetColor();
            break;
        }
        if (header.flag == 0 && cksum((u_short*)Buffer, length - sizeof(header)))
        {
            //判断是否超出范围；这里需要注意两种特殊情况
            //一是接受的SEQ为0，1…… 但是窗口位置在254，255……；这种时候应该能够接受并保存对应数据
            //二是接受的SEQ为
            if (((header.SEQ-windowspos< 0 && -header.SEQ+windowspos <= max_windows_size)||
                (header.SEQ-windowspos>0 && (header.SEQ-255+windowspos)<=max_windows_size)))
            {
                
                setColor(34);
                std::cout <<"Outdate Packet. Ignore! "<< endl;
                resetColor();
                continue;//丢弃该数据包
            }
            else if (( header.SEQ-windowspos> max_windows_size)||
                (header.SEQ-windowspos<0 && (header.SEQ+255-windowspos)>=max_windows_size))
            {
                
                setColor(33);
                std::cout <<"Advamced packet. Windows is full. Ignore!"<< endl;
                resetColor();
                continue;//丢弃该数据包
            }
            if(header.SEQ==windowspos){
                seq = int(header.SEQ);
                if (seq > 255)
                {
                    seq = seq - 256;
                }

                // 输出文本行
                std::cout << "Recieve windows: ["<<windowspos<<"……"<<windowspos+max_windows_size<<"]" << std::endl;            
                // 假设从第18列开始
                int startColumn = 19;
            
                // 输出换行符和空格以对齐"^"
                std::cout << std::setw(startColumn) << '^' << std::endl;

                 //取出buffer中的内容
                std::cout << "Receive message " << length - sizeof(header) << " bytes! "<<" SEQ : " << int(header.SEQ) << " SUM:" << int(header.sum) << endl;
                char* temp = new char[length - sizeof(header)];
                memcpy(temp, Buffer + sizeof(header), length - sizeof(header));
                //cout << "size" << sizeof(message) << endl;
                memcpy(message + all, temp, length - sizeof(header));
                all = all + int(header.datasize);
                 //返回ACK
                header.flag = ACK;
                header.datasize = 0;
                header.SEQ = (unsigned char)seq;
                header.sum = 0;
                u_short temp1 = cksum((u_short*)&header, sizeof(header));
                header.sum = temp1;
                memcpy(Buffer, &header, sizeof(header));
                //重发该包的ACK
                sendto(sockServ, Buffer, sizeof(header), 0, (sockaddr*)&ClientAddr, ClientAddrLen);
                setColor(33);
                std::cout << "Send to Clinet ACK. Head:" << windowspos << " SEQ:" << (int)header.SEQ << endl;
                resetColor();
            
                windowspos=windowspos+1>255?0:windowspos+1;
            }
            else{
                // 输出文本行
                std::cout << "Receive windows: ["<<windowspos<<"……"<<windowspos+max_windows_size<<"]" << std::endl;            
                // 假设从第20列开始
                int startColumn = 20;
            
                // 输出换行符和空格以对齐"^"
                std::cout << std::setw(startColumn) << '^' << std::endl;

                 //取出buffer中的内容
                std::cout <<"Windows head is "<<windowspos<< ". Receive message`s" <<" SEQ : " << int(header.SEQ) << " SUM:" << int(header.sum) << endl;
                char* temp = new char[length - sizeof(header)];
                memcpy(temp, Buffer + sizeof(header), length - sizeof(header));
                //cout << "size" << sizeof(message) << endl;
                memcpy(message + all+ MAXSIZE *(header.SEQ-seq), temp, length - sizeof(header));
                 //返回ACK
                header.flag = ACK;
                header.datasize = 0;
                header.SEQ = (unsigned char)seq;
                header.sum = 0;
                u_short temp1 = cksum((u_short*)&header, sizeof(header));
                header.sum = temp1;
                memcpy(Buffer, &header, sizeof(header));
                //重发该包的ACK
                sendto(sockServ, Buffer, sizeof(header), 0, (sockaddr*)&ClientAddr, ClientAddrLen);
                setColor(33);
                std::cout << "Send to Clinet ACK:" << (int)header.SEQ << " SEQ:" << (int)header.SEQ << endl;
                resetColor();
            
            }
        }
    }
    //发送OVER信息
    header.flag = OVER;
    header.sum = 0;
    u_short temp = cksum((u_short*)&header, sizeof(header));
    header.sum = temp;
    memcpy(Buffer, &header, sizeof(header));
    if (sendto(sockServ, Buffer, sizeof(header), 0, (sockaddr*)&ClientAddr, ClientAddrLen) == -1)
    {
        return -1;
    }
    return all;
}


int disConnect(SOCKET& sockServ, SOCKADDR_IN& ClientAddr, int& ClientAddrLen)
{
    HEADER header;
    char* Buffer = new char [sizeof(header)];
    while (1 == 1) 
    {
        int length = recvfrom(sockServ, Buffer, sizeof(header) + MAXSIZE, 0, (sockaddr*)&ClientAddr, &ClientAddrLen);//接收报文长度
        memcpy(&header, Buffer, sizeof(header));
        if (header.flag == FIN && cksum((u_short*)&header, sizeof(header)) == 0)
        {
            cout << "成功接收第一次挥手信息" << endl;
            break;
        }
    }
    //发送第二次挥手信息
    header.flag = ACK;
    header.sum = 0;
    u_short temp = cksum((u_short*)&header, sizeof(header));
    header.sum = temp;
    memcpy(Buffer, &header, sizeof(header));
    if (sendto(sockServ, Buffer, sizeof(header), 0, (sockaddr*)&ClientAddr, ClientAddrLen) == -1)
    {
        return -1;
    }
    clock_t start = clock();//记录第二次挥手发送时间

    //接收第三次挥手
    while (recvfrom(sockServ, Buffer, sizeof(header), 0, (sockaddr*)&ClientAddr, &ClientAddrLen) <= 0)
    {
        if (clock() - start > MAX_TIME)
        {
            header.flag = ACK;
            header.sum = 0;
            u_short temp = cksum((u_short*)&header, sizeof(header));
            header.flag = temp;
            memcpy(Buffer, &header, sizeof(header));
            if (sendto(sockServ, Buffer, sizeof(header), 0, (sockaddr*)&ClientAddr, ClientAddrLen) == -1)
            {
                return -1;
            }
            cout << "第二次挥手超时，正在进行重传" << endl;
        }
    }

    HEADER temp1;
    memcpy(&temp1, Buffer, sizeof(header));
    if (temp1.flag == FIN_ACK && cksum((u_short*)&temp1, sizeof(temp1) == 0))
    {
        cout << "成功接收第三次挥手" << endl;
    }
    else
    {
        cout << "发生错误,客户端关闭！" << endl;
        return -1;
    }

    //发送第四次挥手信息
    header.flag = FIN_ACK;
    header.sum = 0;
    temp = cksum((u_short*)&header, sizeof(header));
    header.sum = temp;
    memcpy(Buffer, &header, sizeof(header));
    if (sendto(sockServ, Buffer, sizeof(header), 0, (sockaddr*)&ClientAddr, ClientAddrLen) == -1)
    {
        cout << "发生错误,客户端关闭！" << endl;
        return -1;
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
    server_addr.sin_port = htons(2457);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    SOCKADDR_IN client_addr;

    server = socket(AF_INET, SOCK_DGRAM, 0);
    bind(server, (SOCKADDR*)&server_addr, sizeof(server_addr));//绑定套接字，进入监听状态
    setColor(31);
    cout << "进入监听状态，等待客户端上线" << endl;
    resetColor();
    int len = sizeof(server_addr);
    //建立连接
    Connect(server, server_addr, len);
     while(true){
        char* name = new char[50];
        char* data = new char[20000000];
        int namelen =  RecvMessage(server, server_addr, len, name);
        if(namelen == 0){
            setColor(31);
            cout << "客户端已断开连接" << endl;
            resetColor();
            delete name;
            delete data;
            break;
        }
        name[namelen] = '\0'; // 确保字符串终止
        int datalen = RecvMessage(server, server_addr, len, data);
        char* fileName = strrchr(name, '\\');
        if (fileName) {
        fileName++; // 跳过反斜杠，指向文件名部分
        } 
        string a= "F:\\MYCODES\\ComputerNetClass\\Lab3\\";
        a+=fileName;
        cout << "完整路径: " << a << endl;
        ofstream fout(a.c_str(), ofstream::binary);
        for (int i = 0; i < datalen; i++) 
        {
            fout << data[i];
        }
        fout.close();
        setColor(31);
        cout << "文件已成功下载到当前路径下" << endl;
        resetColor();
        delete[] name;
        delete[] data;
    }
    disConnect(server, server_addr, len);
    system("pause");
}
