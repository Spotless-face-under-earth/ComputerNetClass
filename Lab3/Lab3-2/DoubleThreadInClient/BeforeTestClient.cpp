#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <cstring>
#include <chrono>
#include <unordered_set>
#include <WINSOCK2.h>
#include <ws2tcpip.h>
#include <unistd.h>
#include <fstream>
#include <iomanip>
# include <deque>
#include <algorithm>
#include <iterator>

using namespace std;

// 命令行界面优化
void setColor(int color_code) {
    std::cout << "\033[" << color_code << "m";
}

void resetColor() {
    std::cout << "\033[0m";
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

constexpr int HEADER_SIZE = sizeof(HEADER);   // 自定义Header大小
constexpr int MAX_SIZE = 1024; // 每个数据包大小
constexpr int MAX_SEQ_NUM = 256; // 最大序号
constexpr int WINDOW_SIZE = 4;   // 滑动窗口大小
constexpr int PORT = 2456;       // 服务端端口

const int MAXSIZE = 1024;//传输缓冲区最大长度
const unsigned char SYN = 0x1; //SYN = 1 ACK = 0
const unsigned char ACK = 0x2;//SYN = 0, ACK = 1，FIN = 0
const unsigned char ACK_SYN = 0x3;//SYN = 1, ACK = 1
const unsigned char FIN = 0x4;//FIN = 1 ACK = 0
const unsigned char FIN_ACK = 0x5;
const unsigned char OVER = 0x7;//结束标志
const unsigned char PreOVER = 0x8;//

const unsigned char LANMODE = 0x9;//慢启动阶段
const unsigned char SPLMODE = 0x10;//丢包瞬间，减小窗口大小
const unsigned char ACKMODE = 0x11;//正常发送情况

const int max_windows_size = 4;//最大窗口大小
int initial_windows_size = 1;//初始窗口大小 

double MAX_TIME =0.5 * CLOCKS_PER_SEC;


std::mutex mtx;
std::condition_variable cv;

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

int disConnect(SOCKET& socketClient, SOCKADDR_IN& servAddr, int& servAddrlen)
{
    HEADER header;
    char* Buffer = new char[sizeof(header)];

    u_short sum;

    //进行第一次挥手
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

void send_package(SOCKET& socketClient, SOCKADDR_IN& servAddr, int& servAddrlen, char* message, int len, int order)
{
    u_long mode=1;
    ioctlsocket(socketClient, FIONBIO, &mode);//进入模式
    HEADER header;
    char* buffer = new char[MAXSIZE + sizeof(header)];
    header.datasize = len;
    header.SEQ = u_char(order);//序列号
    memcpy(buffer, &header, sizeof(header));
    memcpy(buffer + sizeof(header), message, len);
    u_short check = cksum((u_short *) buffer, sizeof(header) + len);//计算校验和
    header.sum = check;
    memcpy(buffer, &header, sizeof(header));
    sendto(socketClient, buffer, len + sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen);//发送
    //std::cout << "Send message " << len << " bytes!" << " flag:" << int(header.flag) << " SEQ:" << int(header.SEQ) << " SUM:" << int(header.sum) << std::endl;
    mode = 0;
    ioctlsocket(socketClient, FIONBIO, &mode);//改回阻塞模式
}

void sender(SOCKET& socketClient, SOCKADDR_IN& servAddr, int& servAddrlen, char* message, long long int len,int& resendCount, int& base ,deque<int>& d, mutex& mtx){
    //lock_guard<std::mutex> lock(mtx);
    int packagenum = len / MAXSIZE + (len % MAXSIZE != 0);
    cout<<"Package num:"<<packagenum<<endl;
    int round = 0;
    int seqnum;
    seqnum=base;
    clock_t start=clock();
   
    int i = 0;
    for (i; i < packagenum; i++)
    {        
        
        //send_package(socketClient, servAddr, servAddrlen, message + i * MAXSIZE, i == packagenum - 1? len - (packagenum - 1) * MAXSIZE : MAXSIZE, seqnum,base);
        //std::unique_lock<std::mutex> lock(mtx);
        if((seqnum>=base&&seqnum<base+max_windows_size)||
            (seqnum<base&&seqnum+256-base<max_windows_size))
        {
            
            {
                std::lock_guard<std::mutex> lock(mtx);
                setColor(33);
                // 输出文本行
                //std::cout << "Send windows: ["<<base<<"……"<<base+max_windows_size<<"] " 
                cout<< "Windows head:"<<base<<" Send SEQ:"<<seqnum<< std::endl;  
                d.push_back(i);
                //cout<<"Push SEQ to d:"<<i<<" size of d:"<<d.size()<<endl;          
                resetColor();
                cv.notify_all();
                resendCount=0;

                send_package(socketClient, servAddr, servAddrlen, message + i * MAXSIZE, i == packagenum - 1? len - (packagenum - 1) * MAXSIZE : MAXSIZE, seqnum);
                start=clock();
            }
            
        }
        else{
           
            if(clock()-start >  MAX_TIME){
                //lock_guard<std::mutex> lock(mtx);
                if(resendCount<=3){
                    std::lock_guard<std::mutex> lock(mtx);
                    int prenum=d.front()%256;
                    setColor(33);
                    // 输出文本行
                    std::cout << "Time out. Resend packet`s SEQ: "<< prenum << std::endl;            
                    resetColor();
                    cv.notify_all();

                    send_package(socketClient, servAddr, servAddrlen, message + d.front() * MAXSIZE, i == packagenum - 1? len - (packagenum - 1) * MAXSIZE : MAXSIZE, prenum );
                    
                    resendCount++;
                    i--;
                    start=clock();
                    
                }
                else{
                    std::unique_lock<std::mutex> lock(mtx); 
                    resetColor();
                    cout<<"Sleep. "<<endl;
                    cv.wait(lock, [&]() { return ((seqnum >= base && seqnum < base + WINDOW_SIZE) || 
                                         (base + WINDOW_SIZE > 255 && seqnum < (base + WINDOW_SIZE) % 256)); 
                                          });
                    start=clock();
                    i--;
                }
                continue;
            }
            else{
                i--;
                continue;
            }            
            
        }
        if(seqnum+1>255){
            round++;
        }
        seqnum = (seqnum + 1) % 256; // 确保序列号在 0-255 循环
           
    }
    sleep(2);
    //确保d中缺失的包能被重发
    if(!d.empty()){      
        for (deque<int>::iterator it = d.begin(); it != d.end(); ++it) {
            send_package(socketClient, servAddr, servAddrlen, message + (*it * MAXSIZE), i == packagenum - 1? len - (packagenum - 1) * MAXSIZE : MAXSIZE, *it);
            cout<<"Ready to end. Resend SEQ:"<<*it<<endl;
            // //提前发送结束，以免接收端继续增加长度。
            // if(d.size()==1){
            //     HEADER sendheader;
            //     sendheader.flag = PreOVER;
            //     u_short temp = cksum((u_short*)&sendheader, sizeof(sendheader));
            //     sendheader.sum=temp;
            //     char* Buffer =new char[sizeof(sendheader)];
            //     memcpy(Buffer,&sendheader,sizeof(sendheader));
            //     sendto(socketClient, Buffer, sizeof(sendheader), 0, (sockaddr*)&servAddr, servAddrlen);
            // }
        }
    }
    //等待之前没有确认的报文重发后确认。
    std::unique_lock<std::mutex> lock(mtx);
    setColor(33);
    std::cout << "Thread waiting...\n";
    cv.wait(lock, [&d] { return d.size()== 0; });  // 等待 窗口中的内容发完 变为 true
    std::cout << "Thread resumed!\n";
    resetColor();
//发送结束信息
end:
        //cout<<"Break i="<<i<<endl;
        HEADER header;
        char* Buffer = new char[sizeof(header)];
        header.flag = OVER;
        header.sum = 0;
        u_short temp = cksum((u_short*)&header, sizeof(header));
        header.sum = temp;
        memcpy(Buffer, &header, sizeof(header));
        sendto(socketClient, Buffer, sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen);
        cout << "Send End!" << endl;
        
        setColor(31);
        cout << "对方已成功接收文件!" << endl;
        resetColor();

    // u_long mode = 0;
    // ioctlsocket(socketClient, FIONBIO, &mode);//改回阻塞模式
}

void sendName(SOCKET& socketClient, SOCKADDR_IN& servAddr, int& servAddrlen, char* message, long long int len,int& resendCount, int& base ,mutex& mtx){
    //lock_guard<std::mutex> lock(mtx);
    int packagenum = len / MAXSIZE + (len % MAXSIZE != 0);
    //cout<<"Package num:"<<packagenum<<endl;
    int round = 0;
    int seqnum;
    seqnum=base;
    clock_t start=clock();
   
    int i = 0;
    while(true)
    {        
        
        //send_package(socketClient, servAddr, servAddrlen, message + i * MAXSIZE, i == packagenum - 1? len - (packagenum - 1) * MAXSIZE : MAXSIZE, seqnum,base);
        //std::unique_lock<std::mutex> lock(mtx);
        if((seqnum>=base&&seqnum<base+max_windows_size)||
            (seqnum<base&&seqnum+256-base<max_windows_size))
        {
            resendCount=0;

            send_package(socketClient, servAddr, servAddrlen, message, i == packagenum - 1? len - (packagenum - 1) * MAXSIZE : MAXSIZE, seqnum);
            start=clock();
        }
        else{
           
            if(clock()-start >  MAX_TIME){
                //lock_guard<std::mutex> lock(mtx);
                if(resendCount<=3){
                    std::lock_guard<std::mutex> lock(mtx);
                    // int prenum=d.front()%256;
                    setColor(33);
                    // 输出文本行
                    std::cout << "Time out. Resend packet`s SEQ: "<<base << std::endl;            
                    resetColor();
                    cv.notify_all();

                    send_package(socketClient, servAddr, servAddrlen, message , i == packagenum - 1? len - (packagenum - 1) * MAXSIZE : MAXSIZE, seqnum );
                    
                    resendCount++;
                    i--;
                    start=clock();
                    
                }
                else{
                    std::unique_lock<std::mutex> lock(mtx); 
                    resetColor();
                    cout<<"Sleep. "<<endl;
                    cv.wait(lock, [&]() { return ((seqnum >= base && seqnum < base + WINDOW_SIZE) || 
                                         (base + WINDOW_SIZE > 255 && seqnum < (base + WINDOW_SIZE) % 256)); 
                                          });
                    start=clock();
                    i--;
                }
                continue;
            }
            else{
                i--;
                continue;
            }            
            
        }
        char* recvheader=new char[sizeof(HEADER)];
        if(recvfrom(socketClient, recvheader ,sizeof(HEADER), 0, (struct sockaddr*)&servAddr, &servAddrlen)>=0){
            delete recvheader;
            break;
        }
           
    }

//发送结束信息
end:
        //cout<<"Break i="<<i<<endl;
        HEADER header;
        char* Buffer = new char[sizeof(header)];
        header.flag = OVER;
        header.sum = 0;
        u_short temp = cksum((u_short*)&header, sizeof(header));
        header.sum = temp;
        memcpy(Buffer, &header, sizeof(header));
        sendto(socketClient, Buffer, sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen);
        cout << "Send End!" << endl;
        start = clock();
        while (1 == 1)
        {
            // u_long mode = 1;
            // ioctlsocket(socketClient, FIONBIO, &mode);
            int resendCount=0;
            while (recvfrom(socketClient, Buffer, MAXSIZE, 0, (sockaddr*)&servAddr, &servAddrlen) <= 0)
            {
                if (clock() - start > MAX_TIME)
                {
                    if(resendCount<=5){
                        char* Buffer = new char[sizeof(header)];
                        header.flag = OVER;
                        header.sum = 0;
                        u_short temp = cksum((u_short*)&header, sizeof(header));
                        header.sum = temp;
                        memcpy(Buffer, &header, sizeof(header));
                        sendto(socketClient, Buffer, sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen);
                        cout << "Time Out! ReSend END." << endl;
                        resendCount++;
                        //cout<<"Current Windows head:"<<base<<endl;
                        start = clock();
                    }
                    
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
    
    // u_long mode = 0;
    // ioctlsocket(socketClient, FIONBIO, &mode);//改回阻塞模式
}

// 接收线程
void receiver(SOCKET& socketClient, SOCKADDR_IN& servAddr, int& servAddrlen,int&resendCount, int& base,deque<int>& d, mutex& mtx) {
    // lock_guard<std::mutex> lock(mtx);
    HEADER recvheader;
    char buffer[sizeof(recvheader)] = {0};
    int recvround=0;
    //原打算用来设置轮数增加的量
    //bool setNextround=false;

    while (true) {
        int receivedBytes = recvfrom(socketClient, buffer,sizeof(HEADER), 0, (struct sockaddr*)&servAddr, &servAddrlen);
        if (receivedBytes > 0) {
            memcpy(&recvheader, buffer, sizeof(recvheader));//缓冲区接收到信息，读取
            u_short check = cksum((u_short*)&recvheader, sizeof(recvheader));
            
            if(recvheader.flag==OVER){
                setColor(33);
                cout<<"Recv OVER!"<<endl;
                resetColor();
                //send_package(socketClient, servAddr, servAddrlen, (char*)&recvheader, sizeof(recvheader), recvheader.SEQ);
                break;
            }
                
            if (((recvheader.SEQ >= base && recvheader.SEQ < base + WINDOW_SIZE)||
                (recvheader.SEQ<base&&recvheader.SEQ+MAX_SEQ_NUM-base<WINDOW_SIZE))&& recvheader.flag==ACK) {
                    //对整个作用域的操作进行加锁，离开作用域后自动解锁。
                std::lock_guard<std::mutex> lock(mtx);
                
                cout << "Send has been confirmed! Flag:" << int(recvheader.flag)<< " SEQ:" << int(recvheader.SEQ) <<" Base:"<<base<< endl;
                resetColor();
                
                if(base == recvheader.SEQ){
                    //这里也很需要注意，很容易犯错。
                    // if(d.empty()){
                    //     base=(base+1>255?0:base+1); 
                    // }
                    // else if(d.size()!=1){
                    //     base=d.front();
                    // }
                    //base=(base+1>255?0:base+1); 

                    //这样设置来不及更新base，导致q找到的数据包错误。
                    //if(base==255)recvround++;

                    resendCount=0;
                    if(!d.empty()) {
                        //cout<<"Pop SEQ from d:"<<d.front()<<" size of d:"<<d.size()<<endl;
                        d.pop_front();
                        // if(d.size()>=2){
                        //     if(d.at(1)%256==255){
                        //         //setNextround=true;
                        //     }
                        // }
                    }
                    if(d.empty()){
                        base=(base+1>255?0:base+1); 
                    }
                    else{
                        base=d.front()%256;
                    }                 
                    cv.notify_all();
                }
                else{
                    // vector<int> temp;
                    // while(!d.empty()){
                    //     temp.push_back(d.front());
                    //     d.pop_front();
                    // }
                    if((recvheader.SEQ==0 &&  d.back()%256==255)||(recvheader.SEQ==1 &&  d.back()%256==0)){
                        recvround++;
                        //setNextround=false;
                    }
                    auto it = std::find(d.begin(), d.end(), int(recvheader.SEQ)+recvround*256);
                    if((*it)%256==255){ 
                        cout<<"recvHead:"<<int(recvheader.SEQ)<<" recvRound:"<<recvround<<endl;
                        recvround++;  
                    }                
                    //cout<<"Pop SEQ from d:"<<*it<<". size of d:"<<d.size()<<endl;
                    d.erase(it);
                    resendCount=0; 
                }
                cv.notify_all();
                
            }
            else{
                std::lock_guard<std::mutex> lock(mtx);
                
                cout << "Recv packet:" << int(recvheader.flag)<< " SEQ:" << int(recvheader.SEQ) <<" Base:"<<base<< endl;
                resetColor();

                cv.notify_all();
                // send_package(socketClient, servAddr, servAddrlen, message + (recvheader.SEQ) * MAXSIZE, recvheader.datasize, recvheader.SEQ);
            }
            
        }
        else{
            cout<<"Recv Error!"<<endl;
            break;
        }
    }
}

int main() {
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
        cout<<"Connetion Wrong."<<endl;
        return 0;
    }

    // 读取文件
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

    // 滑动窗口状态变量
    static int base = 0;                         // 滑动窗口起始序号
    int back=0;                                 // 已确认的包序号集合
    int resendCount=0;                          // 重传次数
    deque<int> d;                               

    clock_t SendTime=clock();
    sendName(server, server_addr, len,(char*)(filename.c_str()), filename.length(),resendCount, base, mtx);
    cout<<"已告知客户端。发送文件大小为:"<<index<<" Bytes"<<endl;

    sleep(2);
    base=0;


    deque<int> sendbutnotrcvqueue;           // 发送但未确认的包序号集合
    // 创建发送和接收线程
     std::thread sendThread(
        sender,                       // 函数名
        std::ref(server),             // SOCKET& 需要 std::ref
        std::ref(server_addr),        // sockaddr_in& 需要 std::ref
        std::ref(len),        // int& 需要 std::ref
        buffer,                       // char* 不需要 std::ref
        index,                          // long long int 直接传值
        std::ref(resendCount),        // int& 需要 std::ref
        std::ref(base),                // int& 需要 std::ref
        std::ref(sendbutnotrcvqueue),               // 传递
        std::ref(mtx)               // 传递 mtx 引用
    );
    std::thread recvThread(
        receiver,                       // 函数名
        std::ref(server),             // SOCKET& 需要 std::ref
        std::ref(server_addr),        // sockaddr_in& 需要 std::ref
        std::ref(len),        // int& 需要 std::ref
        std::ref(resendCount),        // int& 需要 std::ref
        std::ref(base),                // int& 需要 std::ref
        std::ref(sendbutnotrcvqueue),               // 传递
        std::ref(mtx)           // 传递 mtx 引用
    );


    recvThread.detach(); // 接收线程可以持续运行
    sendThread.join();
    

    disConnect(server,server_addr,len);

    setColor(31);
    cout<<"文件发送时间："<<(double)(clock()-SendTime)/CLOCKS_PER_SEC<<"s. 传输带宽："<<index/(double)(clock()-SendTime)<<"Bytes/s. "<<endl;
    resetColor();

    close(server);
    system("pause");
    return 0;
}
