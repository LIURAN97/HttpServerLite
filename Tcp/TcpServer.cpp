#include "TcpServer.h"

#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>

#include <HttpBuffer.h>

#define LISTEN_NUM 20
#define EPOLL_MAX_FD 256
#define READ_BUFFER_SIZE 4096


TcpServer::TcpServer(int listen_port)
    : m_listen_port(listen_port)
{
}

TcpServer::TcpServer(TcpServer &&server) noexcept
{
    if (this != &server)
    {
        m_listen_port = server.m_listen_port;
        m_io_thread = std::move(server.m_io_thread);
        //m_connection_fd_map = server.m_connection_fd_map;
    }
    
}

TcpServer &TcpServer::operator=(TcpServer &&server) noexcept
{
    if (this != &server)
    {
        m_listen_port = server.m_listen_port;
        m_io_thread = std::move(server.m_io_thread);
        //m_connection_fd_map = server.m_connection_fd_map;
    }

    return *this;
}

TcpServer::~TcpServer()
{
}

bool TcpServer::Start()
{
    if (m_loop_running)
    {
        return false;
    }

    if (!Listen())
    {
        return false;
    }

    m_loop_running = true;
    m_io_thread = std::thread(&TcpServer::IOThreadProc, this);
    pthread_setname_np(m_io_thread.native_handle(), "***IO thraed***");

    return true;
}

bool TcpServer::Stop()
{
    if (m_loop_running)
    {
        m_loop_running = false;
        if (m_io_thread.joinable())
        {
            m_io_thread.join();
        }
    }

    if (m_listen_sock > 0)
    {
        close(m_listen_sock);
    }

    return true;
}

bool TcpServer::Listen()
{
    int listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock < 0)
    {
        printf("create socket fail!\n");
        return false;
    }

    int opts = fcntl(listen_sock, F_GETFL);
    if (opts < 0)
    {
        printf("get fcntl fail! opts:%d\n", opts);
        return false;
    }
    
    opts = opts | O_NONBLOCK;
    if (fcntl(listen_sock, F_SETFL, opts) < 0)
    {
        printf("set fd:%d non block fail!\n", listen_sock);
        return false;
    }
    
    struct sockaddr_in server_addr;
    memset(&server_addr, 0 , sizeof(sockaddr_in));
    server_addr.sin_port = htons(m_listen_port);
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(listen_sock, (const sockaddr *)&server_addr, sizeof(server_addr)) != 0)
    {
        printf("bind socket fail! error:%d\n", errno);
        return false;
    }

    if (listen(listen_sock, LISTEN_NUM) != 0)
    {
        printf("listen on fd:%d fail! error:%d\n", listen_sock, errno);
    }

    printf("port:%d is listenning!\n", m_listen_port);
    m_listen_sock = listen_sock;
    return true;
}

void TcpServer::IOThreadProc()
{
    int epfd = epoll_create(256);

    epoll_event server_event;
    memset(&server_event, 0, sizeof(epoll_event));
    server_event.events |= EPOLLIN;
    server_event.data.fd = m_listen_sock;
    epoll_ctl(epfd, EPOLL_CTL_ADD, m_listen_sock, &server_event);

    epoll_event events[EPOLL_MAX_FD];
    
    int event_count = 0;

    while (m_loop_running)
    {
        memset(events, 0, sizeof(events));
        event_count = epoll_wait(epfd, events, EPOLL_MAX_FD, 200);
        if (event_count <= 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR || event_count == 0)
            {
                continue;
            }

            printf("epoll_wait error! errno %d!\n", errno);
            break;
        }
        else
        {
            for (size_t i = 0; i < event_count; ++i)
            {
                epoll_event cur_ev = events[i];
                if (cur_ev.data.fd == m_listen_sock) // 表示有新的连接过来
                {
                    sockaddr_in peer_addr = {0};
                    socklen_t peer_addr_len = sizeof(peer_addr);

                    int conn_sock = accept(cur_ev.data.fd, (sockaddr *)&peer_addr, &peer_addr_len);
                    //printf("Connection establish, socket:%d peer address : %s:%d \n", conn_sock, inet_ntoa(peer_addr.sin_addr), peer_addr.sin_port);
                    m_conn_cb(inet_ntoa(peer_addr.sin_addr), static_cast<int>(peer_addr.sin_port));

                    epoll_event new_conn_event = {0};
                    new_conn_event.events = EPOLLIN | EPOLLPRI | EPOLLOUT | EPOLLET; //可读可写，并且是边缘触发
                    new_conn_event.data.fd = conn_sock;
                    epoll_ctl(epfd, EPOLL_CTL_ADD, conn_sock, &new_conn_event); //将新的链接加入epoll中进行监控
                }
                else if (cur_ev.events & EPOLLIN || cur_ev.events & EPOLLPRI)
                {
                    char msg[READ_BUFFER_SIZE] = {0};
                    int msg_len = 0;

                    int recv_count = recv(cur_ev.data.fd, msg, sizeof(msg), MSG_DONTWAIT);
                    while (recv_count != -1)
                    {
                        if (recv_count == 0) // recv_count == 0表示连接断开
                        {
                            sockaddr_in peer_addr = {0};
                            socklen_t peer_addr_len = sizeof(peer_addr);
                            getpeername(cur_ev.data.fd, (sockaddr *)&peer_addr, &peer_addr_len);
                            printf("peer:%s had close the connection!\n", inet_ntoa(peer_addr.sin_addr));
                            close(cur_ev.data.fd);
                        }

                        char *msg_empty_buffer = msg + recv_count;
                        msg_len += recv_count;

                        recv_count = recv(cur_ev.data.fd, msg_empty_buffer, 4096 - msg_len, MSG_DONTWAIT);
                    }

                    //正常情况下需要不停recv直到获得错误码EAGIN / EWOULDBLOCK
                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                    {
                        m_read_cb(msg);
                    }
                }
                else if (cur_ev.events & EPOLLOUT)
                {
                    std::string msg;
                    m_wirte_cb(msg);
                    int msg_length = msg.length();

                    int write_count = send(cur_ev.data.fd, msg.c_str(), msg_length, MSG_DONTWAIT);
                    while (write_count < msg_length)
                    {
                        msg = msg.substr(write_count - 1, msg_length - 1);
                        msg_length -= write_count;
                        write_count = send(cur_ev.data.fd, msg.c_str(), msg_length, MSG_DONTWAIT);
                    }
                }
            }
        }
    }
}
