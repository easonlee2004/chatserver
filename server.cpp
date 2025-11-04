#include "server.h"

const int MAX_CONN = 1024;

// 用于描述客户端的结构体
struct Client
{
    int sockfd;
    std::string name;
};

int main()
{
    struct sockaddr_in servaddr;

    // 创建epoll实例
    int epfd = epoll_create(1);
    if (epfd == -1)
    {
        perror("epoll create error");
        exit(1);
    }

    // 创建监听socket
    int lfd = socket(AF_INET, SOCK_STREAM, 0);
    if (lfd == -1)
    {
        perror("socket error");
        exit(1);
    }

    // bind socket
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(DEFAULT_PORT); // 自己定义的端口宏
    if (bind(lfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) == -1)
    {
        perror("bind error");
        exit(1);
    }

    // 监听socket
    if (listen(lfd, MAX_CONN) == -1)
    {
        perror("listen error");
        exit(1);
    }

    // 将监听的socket加入epoll
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = lfd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, lfd, &ev))
    {
        perror("epoll ctl error");
        exit(1);
    }

    // 使用map保存客户端信息
    std::map<int, Client> clients;

    // 循环监听
    while (1)
    {
        struct epoll_event evs[MAX_CONN];
        int n = epoll_wait(epfd, evs, MAX_CONN, -1);
        if (n < 0)
        {
            perror("epoll wait error");
            exit(1);
        }

        for (int i = 0; i < n; i++) // n是epoll_wait返回的可用fd个数
        {
            int fd = evs[i].data.fd;
            // 如果是监听sock的消息
            if (fd == lfd)
            {
				struct sockaddr_in client_addr;
                socklen_t client_addr_len = sizeof(client_addr);
                // accept
				int cfd = accept(lfd, (struct sockaddr*)&client_addr, &client_addr_len);
                if (cfd == -1)
                {
                    perror("accept error");
                    continue;
                }

                struct epoll_event ev_client;
                ev_client.events = EPOLLIN; // 检测客户端是否有消息
                ev_client.data.fd = cfd;

                if (epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &ev_client))
                {
                    perror("epoll ctl error");
                    continue;
                }

                Client client;
                client.sockfd = cfd;
                client.name = "";
                clients[client.sockfd] = client;
            }
            // 如果是客户端消息
            else
            {
                char buffer[1024];
                int n = recv(fd, buffer, 1024, 0); // n为真实接受的字节数
                if (n < 0)
                {
                    // TODO
                    break;
                }
                else if (n == 0) // 表示客户端断开
                {
                    // close fd
                    close(fd);
                    // 从epfd中移除
                    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, 0);
                    // 从map中删除
                    clients.erase(fd);
                }
                else // n为正数
                {
                    std::string msg(buffer, n);

                    /*
                    * 判断一下name是否为空
                    * 如果空说明msg是用户名
                    * 如果非空则msg是消息
                    */
                    if (clients[fd].name == "")
                    {
                        clients[fd].name = msg;
                    }
                    else
                    {
                        // 把消息发给其他所有人
                        std::string name = clients[fd].name;
                        for (auto& c : clients)
                        {
                            if (c.first != fd)
                            {
                                send(c.first,
                                    ("[" + name + "]" + ": " + msg).c_str(),
                                    ("[" + name + "]" + ": " + msg).size(),
                                    0
                                );
                            }
                        }
                    }
                }
            }
        }
    }

    close(epfd);
    close(lfd);

    return 0;
}