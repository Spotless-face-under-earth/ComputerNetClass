#include <iostream>
#include <WINSOCK2.h>
#include <iomanip>
#include <time.h>
#include <fstream>
#include <deque>
#include <mutex>
#pragma comment(lib, "ws2_32.lib")
using namespace std;


const int MAXSIZE = 1024;//传输缓冲区最大长度
const unsigned char SYN = 0x1; //SYN = 1 ACK = 0
const unsigned char ACK = 0x2;//SYN = 0, ACK = 1
const unsigned char ACK_SYN = 0x3;//SYN = 1, ACK = 1
const unsigned char FIN = 0x4;//FIN = 1 ACK = 0
const unsigned char FIN_ACK = 0x5;//FIN = 1 ACK = 0
const unsigned char OVER = 0x7;//结束标志
const unsigned char PreOVER=0X8;

int max_windows_size=4;
int initial_windows_size=1;

double MAX_TIME = 0.8 * CLOCKS_PER_SEC;

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

int RecvMessage(SOCKET& sockServ, SOCKADDR_IN& ClientAddr, int& ClientAddrLen, char *message,deque<int>& d)
{
    long int all = 0;//文件长度
    HEADER header;
    char* Buffer = new char[MAXSIZE + sizeof(header)];
    int seq = 0;
    int index = 0;
    int windowspos=0;
    //判断正常接收但是并没在base中更新的报文数量。
    //int recvNum=0;
    //
    deque<int> eachBlank;
    int constSeq=0;
    int beforeSeq=0;
    while (1 == 1)
    {
        int length = recvfrom(sockServ, Buffer, sizeof(header) + MAXSIZE, 0, (sockaddr*)&ClientAddr, &ClientAddrLen);//接收报文长度
        //cout << length << endl;
        memcpy(&header, Buffer, sizeof(header));
        time_t start = clock();
        //判断是否是结束
        if (header.flag == OVER && cksum((u_short*)&header, sizeof(header)) == 0)
        {
            setColor(31);
            std::cout << "文件接收完毕" << endl;
            resetColor();

            break;
        }
        // //结束前的前一个报文
        // if(header.flag==PreOVER && d.size()==1)
        // {
            
        //     int finallength = recvfrom(sockServ, Buffer, sizeof(header) + MAXSIZE, 0, (sockaddr*)&ClientAddr, &ClientAddrLen);//接收报文长度
        //     memcpy(&header, Buffer, sizeof(header));
        //     seq = int(header.SEQ);
        //     if (seq > 255)
        //     {
        //         seq = seq - 256;
        //     }
        //     memcpy(message + all,Buffer+sizeof(header),length-sizeof(header));
        //     //返回ACK
        //     header.flag = ACK;
        //     header.datasize = 0;
        //     header.SEQ = seq;
        //     u_short temp1 = cksum((u_short*)&header, sizeof(header));
        //     header.sum = temp1;
        //     memcpy(Buffer, &header, sizeof(header));
            
        //     //发该包的ACK
        //     sendto(sockServ, Buffer, sizeof(header), 0, (sockaddr*)&ClientAddr, ClientAddrLen);
        //     setColor(33);
        //     std::cout << "Ready to end. Send SEQ:" << (int)header.SEQ << endl;
        //     resetColor();

        //     d.pop_front();

        //     continue;
        // }
        //正常文件内容的报文。
        if (header.flag == 0 && length>0)
        {
            //cout<<"flag=0;"<<endl;

            seq = int(header.SEQ);
            if (seq > 255)
            {
                seq = seq - 256;
            }

            //判断是否超出范围；这里需要注意两种特殊情况
            //一是接受的SEQ为0，1…… 但是窗口位置在254，255……；这种时候应该能够接受并保存对应数据
            //二是接受的SEQ为
            //接受包是之前的SEQ，说明之前的包客户端还没来得及接受，重传ACK
            if(seq == windowspos)
            {
                
                 //取出buffer中的内容
                std::cout << "Receive message " << length - sizeof(header) << " bytes! "<<" SEQ:" << int(header.SEQ) << " SUM:" << int(header.sum) << endl;
                //这里注意最开始的时候deque为空，在这里犯过错
                if(d.empty()){
                    memcpy(message+all,Buffer+sizeof(header),length-sizeof(header));
                    //返回ACK
                    header.flag = ACK;
                    header.datasize = 0;
                    header.SEQ = seq;
                    u_short temp1 = cksum((u_short*)&header, sizeof(header));
                    header.sum = temp1;
                    memcpy(Buffer, &header, sizeof(header));
                    
                    //发该包的ACK
                    sendto(sockServ, Buffer, sizeof(header), 0, (sockaddr*)&ClientAddr, ClientAddrLen);
                    setColor(33);
                    std::cout << "Send to Clinet ACK. Head:" << windowspos << " SEQ:" << (int)header.SEQ << endl;
                    resetColor();

                    all+=length-sizeof(header);     
                    windowspos=seq+1>255?0:seq+1;
                }
                //这里的逻辑是，如果出现丢包，前一个报文的seq会被d存入。
                //这里还有个问题，因为报文可能在队列中被漏了好多个，需要合理规划每一个报文和当前seq的间隔到底是多少。
                else{
                    int dis=length-sizeof(header);
                    if(d.size()>=2){
                        if((d.at(1)-d.at(0))>=0){
                            dis=(d.at(1)-d.at(0))*(length-sizeof(header));
                            //cout<<"Distance:"<<dis<<endl;
                        }
                        else{
                            dis=(d.at(1)+256-d.at(0))*(length-sizeof(header));
                            //cout<<"Distance:"<<dis<<endl;
                        }
                    }
                    else{
                        if(beforeSeq-windowspos>0){
                            dis=beforeSeq-windowspos;
                        }
                        else{
                            dis=windowspos+256-beforeSeq;
                        }
                        cout<<"Error in dis culculate."<<endl;
                        system("pause");
                        // break;
                    }
                    //all += recvNum*(length-sizeof(header));
                    // cout<<"Total package size:"<<all<<endl;
                    memcpy(message+all,Buffer+sizeof(header),length-sizeof(header));

                    all+=dis;
                    //返回ACK
                    header.flag = ACK;
                    header.datasize = 0;
                    header.SEQ = seq;
                    header.sum = 0;
                    u_short temp1 = cksum((u_short*)&header, sizeof(header));
                    header.sum = temp1;
                    memcpy(Buffer, &header, sizeof(header));
                    
                    //发该包的ACK
                    sendto(sockServ, Buffer, sizeof(header), 0, (sockaddr*)&ClientAddr, ClientAddrLen);
                    setColor(33);
                    std::cout << "Send to Clinet ACK. Head:" << windowspos << " SEQ:" << (int)header.SEQ << endl;
                    resetColor(); 
                    d.pop_front();
                    if(!d.empty()){
                        windowspos=d.front();
                    }
                    else{
                        windowspos=seq+1>255?0:seq+1;
                    }
                }    
                //recvNum=0;
            }
            // 这里逻辑判断还是有点弱，应该判断一下之前是否接受过这样的包。但是实测下来不会重发。
            else if((seq>windowspos && seq-windowspos<max_windows_size)||
                    (seq<windowspos && seq+256-windowspos<max_windows_size)){

                //recvNum++;
            
                //将接收到的数据包存起来。
                char* temp = new char[length - sizeof(header)];
                memcpy(temp, Buffer + sizeof(header), length - sizeof(header));
                //cout << "size" << sizeof(message) << endl;
                if(header.SEQ > windowspos){
                    memcpy(message + all + (header.SEQ-windowspos)*MAXSIZE, temp, length - sizeof(header));
                }
                else{
                    memcpy(message + all+(header.SEQ-windowspos+256)*MAXSIZE, temp, length - sizeof(header));
                }

                //recvNum++;

                // memcpy(message + all+(header.SEQ-windowspos)*MAXSIZE, temp, length - sizeof(header));
                //d.push_back(int(header.SEQ));
                //这里对d的队列的操作也很容易出错。
                if(d.empty()){
                    if(int(header.SEQ)-1>=0){
                        d.push_back(int(header.SEQ)-1);
                    }
                    else{
                        d.push_back(255);
                    }
                   // constSeq=0;
                   // eachBlank[constSeq]=0;
                    //cout<<"Push SEQ to d:"<<int(header.SEQ)-1<<" size of d:"<<d.size()<<endl;
                }
                else if(d.size()==1){
                    //eachBlank[constSeq]++;
                    d.push_back(int(header.SEQ));
                    //cout<<"Push SEQ to d:"<<int(header.SEQ)<<" size of d:"<<d.size()<<endl;
                }
                else if((d.back()==int(header.SEQ)-1)||(d.back()==int(header.SEQ)-1+256)){
                    //eachBlank[constSeq]++;
                    d.pop_back();
                    d.push_back(int(header.SEQ));
                    //cout<<"Push SEQ to d:"<<int(header.SEQ)<<" size of d:"<<d.size()<<endl;
                }
                
                else{
                    //constSeq++;
                    d.pop_back();
                    if(int(header.SEQ)-1>=0){
                        d.push_back(int(header.SEQ)-1);
                    }
                    else{
                        d.push_back(int(header.SEQ)-1+256);
                    }
                    //cout<<"Join lost packet. d`s second element:"<<d.at(1)<<" size of d:"<<d.size()<<endl;
                    d.push_back(int(header.SEQ));
                    
                }
                
                delete[] temp;
                
                // 这里需要返回ACK，留作send端的重发判断。
                header.flag = ACK;
                header.datasize = 0;
                header.SEQ = seq;
                header.sum = 0;
                u_short temp1 = cksum((u_short*)&header, sizeof(header));
                header.sum = temp1;
                memcpy(Buffer, &header, sizeof(header));

                //发该包的ACK
                sendto(sockServ, Buffer, sizeof(header), 0, (sockaddr*)&ClientAddr, ClientAddrLen);
                setColor(33);
                std::cout << "Send to Advanced Clinet ACK. Head:" << windowspos <<" SEQ:"<<int(header.SEQ) << endl;
                resetColor();

                //cout<<"Advanced packet received and stored. SEQ:"<<int(header.SEQ)<<endl;
            }

            // if(clock()-start > MAX_TIME){
            //     header.flag = ACK;
            //     header.datasize = 0;
            //     header.SEQ = windowspos-1;
            //     header.sum = 0;
            //     u_short temp1 = cksum((u_short*)&header, sizeof(header));
            //     header.sum = temp1;
            //     memcpy(Buffer, &header, sizeof(header));
            //     //发该包的ACK
            //     sendto(sockServ, Buffer, sizeof(header), 0, (sockaddr*)&ClientAddr, ClientAddrLen);
            //     setColor(33);
            //     std::cout << "Resend to Clinet ACK. Head:" << windowspos  << endl;
            //     resetColor();
            // }
        
        }
        beforeSeq=header.SEQ;
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

// int SendMessage(SOCKET& sockServ, SOCKADDR_IN& ClientAddr, int& ClientAddrLen, int& windowshead,deque<int>& d, mutex& mtx)
// {
//     HEADER header;
//     char* Buffer = new char[sizeof(header)];
//     int seqnum = 0;
//     int round = 0;
//     int base = 0;
//     int i = 0;
//     int checkcount = 0;
//     while (i < len / MAXSIZE + 1)
//     {
//         if (i == len / MAXSIZE)
//         {
//             header.datasize = len - i * MAXSIZE;
//         }
//         else
//         {
//             header.datasize = MAXSIZE;
//         }
//         header.flag = 0;
//         header.SEQ = u_char(seqnum);
//         memcpy(Buffer, &header, sizeof(header));
//         memcpy(Buffer + sizeof(header), message + i * MAXSIZE, header.datasize);
//         u_short check = cksum((u_short*)Buffer, sizeof(header) + header.datasize);
//         header.sum = check;
//         memcpy(Buffer, &header, sizeof(header));
//         sendto(sockServ, Buffer, header.datasize + sizeof(header), 0, (sockaddr*)&ClientAddr, ClientAddrLen);
//         std::cout << "Send message " << header.datasize << " bytes! " << " SEQ:" << int(header.SEQ) << " SUM:" << int(header.sum) << std::endl;
//         i++;
//         seqnum = (seqnum + 1) % 256;
//     }
//     //发送结束信息
// end:
//     std::cout << "Break i=" << i << std::endl;
//     HEADER header;
//     char* Buffer = new char[sizeof(header)];
//     header.flag = OVER;
//     header.sum = 0;
//     u_short temp = cksum((u_short*)&header, sizeof(header));
//     header.sum = temp;
//     memcpy(Buffer, &header, sizeof(header));
//     sendto(sockServ, Buffer, sizeof(header), 0, (sockaddr*)&ClientAddr, ClientAddrLen);
//     std::cout << "Send End!" << std::endl;
//     return 1;
// }

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
    // while(true){
        deque<int> unACKbutreceived;
        char* name = new char[50];
        char* data = new char[20000000];
        int namelen =  RecvMessage(server, server_addr, len, name,unACKbutreceived);
        if(namelen == 0){
            setColor(31);
            cout << "客户端已断开连接" << endl;
            resetColor();
            delete name;
            delete data;
            //break;
        }
        name[namelen] = '\0'; // 确保字符串终止
        cout<<"文件名已收到."<<endl;
        unACKbutreceived.clear();

        //cout<<unACKbutreceived.size()<<endl;
        int datalen = RecvMessage(server, server_addr, len, data,unACKbutreceived);
        char* fileName = strrchr(name, '\\');
        if (fileName) {
            fileName++; // 跳过反斜杠，指向文件名部分
        } 
        string a= "F:\\MYCODES\\ComputerNetClass\\Lab3\\";
        a+=fileName;
        cout<<"文件大小:"<<datalen<<" Bytes"<<endl;
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
   // }
    disConnect(server, server_addr, len);
    //system("pause");
}
