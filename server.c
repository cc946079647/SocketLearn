#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>
#include <sys/types.h>
#include <arpa/inet.h>
#define IPADDRESS   "127.0.0.1"
#define PORT        8787
#define MAXLINE     1024
#define LISTENQ     5

//函数声明
//创建套接字并进行绑定
static int socket_bind(const char* ip,int port);
//IO多路复用select
static void do_select(int listenfd);
//处理多个连接
static void handle_connection(int *connfds,int num,fd_set *prset,fd_set *pallset);

int main(int argc,char *argv[])
{
    int  listenfd,connfd,sockfd;
    struct sockaddr_in cliaddr;
    socklen_t cliaddrlen;
    listenfd = socket_bind(IPADDRESS,PORT);
    listen(listenfd,LISTENQ);
    do_select(listenfd);
    return 0;
}

static int socket_bind(const char* ip,int port)
{
    int  listenfd;
    struct sockaddr_in servaddr;
    listenfd = socket(AF_INET,SOCK_STREAM,0);
    if (listenfd == -1)
    {
        perror("socket error:");
        exit(1);
    }
    bzero(&servaddr,sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    inet_pton(AF_INET,ip,&servaddr.sin_addr);
    servaddr.sin_port = htons(port);
    if (bind(listenfd,(struct sockaddr*)&servaddr,sizeof(servaddr)) == -1)
    {
        perror("bind error: ");
        exit(1);
    }
    return listenfd;
    //int listen(int socketfd,int backlog)
    //fd为socket 和 bind参数的fd，被称为监听套接字，用于等待连接
    //backlog　可以理解为内核为bind到的地址建立的等待队列的长度
    //等待队列分为　已完成连接队列(对于tcp来说就是３次握手后建立的)和未完成连接队列(处于３次握手过程中)
    //对一个套接字调用listen　就会将这个套接字的状态由CLOSED设置为LISTEN
    //由socket等到的fd是主动套接字，可以通过connect建立连接。调用listen以后这个套接字就是被动套接字，等待连接。
}

static void do_select(int listenfd)
{
    int  connfd,sockfd;
    struct sockaddr_in cliaddr;
    socklen_t cliaddrlen;
    fd_set  rset,allset;
    int maxfd,maxi;
    int i;
    int clientfds[FD_SETSIZE];  //保存客户连接描述符
    int nready;
    //初始化客户连接描述符
    for (i = 0;i < FD_SETSIZE;i++)
        clientfds[i] = -1;
    maxi = -1;
    FD_ZERO(&allset);
    //添加监听描述符
    FD_SET(listenfd,&allset);
    maxfd = listenfd;
    //循环处理
    for ( ; ; )
    {
        rset = allset;
        //获取可用描述符的个数
        /**
         * int select(int maxfdp1,fd_set* readset,fd_set*writeset,fd_set*exceptset,
         *      const struct timeval*timeout)
         * 参数：
         * maxfdp1:感兴趣的所有描述符中最大的描述符+1(内核测试描述符０－maxfdp1,并且只copy感兴趣的描述符对应的fd_set部分以提高性能)
         * readset writeset exceptset　可读　可写　发生异常　３个事件对应的感兴趣的描述符集合
         * timeout　最长等待时间
         *          struct timeval{
         *              long tv_sec; //秒数
         *              long tv_usec;//毫秒数
         *          }
         *         希望无限等待时设为NULL
         *  fd_set是描述符集合，一般一个bit对应于一个描述符。感兴趣/满足事件时bit为１，否则为0。
         *  fd_set由４个宏处理：
         *      void FD_ZERO(fd_set*fdset):初始化fd_set　并设置所有的bit设为０
         *      void FD_SET(int fd,fd_set*fdset) 将fdset中对应描述符fd的bit设为１
         *      void FD_CLR(int fd,fd_set*fdset) 将fdset中对应描述符fd的bit设为０
         *      int FD_ISSET(int fd,fd_set*fd_set) 测试fd_set中对应于描述符fd的bit是否被设置
         * 返回值：
         * 有就绪描述符时返回就绪描述符的数目，超时返回０，出错返回－１
         * 在就绪的情况下，传入的fd_set被修改。满足对应事件的描述符的bit被置为１，可以使用FD_ISSET测试。
         *  这意味着当有就绪描述符返回的情况下，即使需要检测的描述符不变，也仍然需要重新构造fd_set。这造成在用户空间和
         *  内核空间之间fd_set的多次copy
         * **/
        nready = select(maxfd+1,&rset,NULL,NULL,NULL);
        if (nready == -1)
        {
            perror("select error:");
            exit(1);
        }
        //测试监听描述符是否准备好
        if (FD_ISSET(listenfd,&rset))
        {
            cliaddrlen = sizeof(cliaddr);
            //接受新的连接
            //int accept(int sockfd,struct socketaddr*cliaddr,socklen_t*addrlen)
            //　成功返回非负描述符　出错为-1
            //对客户地址和客户地址结构长度不感兴趣的情况下可以设置为NULL
            //返回的套接字是与特定客户链接的套接字，称为已连接套接字
            if ((connfd = accept(listenfd,(struct sockaddr*)&cliaddr,&cliaddrlen)) == -1)
            {
                if (errno == EINTR)
                    continue;
                else
                {
                   perror("accept error:");
                   exit(1);
                }
            }
            fprintf(stdout,"accept a new client: %s:%d\n", inet_ntoa(cliaddr.sin_addr),cliaddr.sin_port);
            //将新的连接描述符添加到数组中
            for (i = 0;i <FD_SETSIZE;i++)
            {
                if (clientfds[i] < 0)
                {
                    clientfds[i] = connfd;
                    break;
                }
            }
            if (i == FD_SETSIZE)
            {
                fprintf(stderr,"too many clients.\n");
                exit(1);
            }
            //将新的描述符添加到读描述符集合中
            FD_SET(connfd,&allset);
            //描述符个数
            maxfd = (connfd > maxfd ? connfd : maxfd);
            //记录客户连接套接字的个数
            maxi = (i > maxi ? i : maxi);
            //只有listenfd就绪　不需要处理其他情况
            if (--nready <= 0)
                continue;
        }
        //处理客户连接
        handle_connection(clientfds,maxi,&rset,&allset);
    }
}

static void handle_connection(int *connfds,int num,fd_set *prset,fd_set *pallset)
{
    int i,n;
    char buf[MAXLINE];
    memset(buf,0,MAXLINE);
    for (i = 0;i <= num;i++)
    {
        if (connfds[i] < 0)
            continue;
        //测试客户描述符是否准备好
        if (FD_ISSET(connfds[i],prset))
        {
            //接收客户端发送的信息
            n = read(connfds[i],buf,MAXLINE);
            if (n == 0)
            {
                close(connfds[i]);
                FD_CLR(connfds[i],pallset);
                connfds[i] = -1;
                continue;
            }
            printf("read msg is: ");
            write(STDOUT_FILENO,buf,n);
            //向客户端发送buf
            write(connfds[i],buf,n);
        }
    }
}