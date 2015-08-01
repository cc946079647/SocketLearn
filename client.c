#include <netinet/in.h>
#include <sys/socket.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/select.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include<arpa/inet.h>

#define MAXLINE     1024
#define IPADDRESS   "127.0.0.1"
#define SERV_PORT   8787

#define max(a,b) (a > b) ? a : b

static void handle_connection(int sockfd);

int main(int argc,char *argv[])
{
    int                 sockfd;
    //ipv4　地址结构体
    /**
     * struct socketaddr_in{
     * //套接字地址结构长度字段，非通用
     *  uint8_t　sinlen;
     *  //协议族号　一般是８bit 无符号整数
     *  sa_family_t sin_family;
     *  //tcp/udp　端口号 16bit 网络字节序
     *  in_port_t sin_port;
     *  //ip地址结构体
     *  struct in_addr sin_addr;
     *  //保留　未用
     *  char sin_zero[8];
     * }
     * struct in_addr{
     *  //32bit ip地址
     *  //以网络字节序存储
     *  in_addr_t s_addr;
     * }
     * 因为不同的网络协议地址结构体不同，为了通用化函数接口，定义了socketaddr作为需要用到地址结构体的相关函数的统一接口类型
     * struct socketaddr{
     * //
     *  uint８_t sa_len;
     *  //协议族号
     *  sa_family sa_family;
     *  //特定协议的地址信息
     *  char sa_data[14]
     * }
     *
     * ipv4　地址结构socketaddr_in大小与通用结构体相同，为16byte。但为了保证强制类型转换后能准确得知之前类型的大小，这些函数都好包括一个
     * 参数，用于表明传入的结构体的字节数。
     */
    struct sockaddr_in  servaddr;
    //socket(int domin,int type,int protocol)
    //AF_INET:ipv4
    //SOCK_STREAM tcp
    //0 use default protocol based on domain and type
    sockfd = socket(AF_INET,SOCK_STREAM,0);
    //将内存对应区域置为０
    bzero(&servaddr,sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    //转换为网络字节序
    //host to network short
    servaddr.sin_port = htons(SERV_PORT);
    //按照协议族　将字符串形式的地址　转换为网络字节序　并写入地址结构体
    //inet_pton(int family,const char*strptr,void* addrptr)
    inet_pton(AF_INET,IPADDRESS,&servaddr.sin_addr);
    connect(sockfd,(struct sockaddr*)&servaddr,sizeof(servaddr));
    //处理连接描述符
    handle_connection(sockfd);
    return 0;
}

static void handle_connection(int sockfd)
{
    char    sendline[MAXLINE],recvline[MAXLINE];
    int     maxfdp,stdineof;
    fd_set  rset;
    int n;
    FD_ZERO(&rset);
    for (; ;)
    {
        //添加标准输入描述符
        FD_SET(STDIN_FILENO,&rset);
        //添加连接描述符
        FD_SET(sockfd,&rset);
        maxfdp = max(STDIN_FILENO,sockfd);
        //进行轮询
        select(maxfdp+1,&rset,NULL,NULL,NULL);
        //测试连接套接字是否准备好
        if (FD_ISSET(sockfd,&rset))
        {
            n = read(sockfd,recvline,MAXLINE);
            if (n == 0)
            {
                    fprintf(stderr,"client: server is closed.\n");
                    close(sockfd);
                    FD_CLR(sockfd,&rset);
            }
            write(STDOUT_FILENO,recvline,n);
        }
        //测试标准输入是否准备好
        if (FD_ISSET(STDIN_FILENO,&rset))
        {
            n = read(STDIN_FILENO,sendline,MAXLINE);
            if (n  == 0)
            {
                FD_CLR(STDIN_FILENO,&rset);
                continue;
            }
            write(sockfd,sendline,n);
        }
    }
}