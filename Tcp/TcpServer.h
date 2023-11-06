/**
 * @ Author: Ran Liu
 * @ Create Time: 2023-11-06 10:22:48
 * @ Modified by: Ran Liu
 * @ Modified time: 2023-11-06 17:28:19
 * @ Description: This file provides a TcpServer class for listening to a port, accepting connections, 
 *                monitoring IO events, and callback the read and write events.
 */

#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include <thread>
#include <atomic>
#include <map>
#include <sys/epoll.h>
#include <functional>

using ConnectionCallback = std::function<void (std::string, int)>;
using ReadCallback = std::function<void (std::string &&)>;
using WriteCallback = std::function<void (std::string &)>;


class TcpServer
{
public:
    explicit TcpServer(int listen_port);
    TcpServer(TcpServer &&server) noexcept;
    TcpServer &operator=(TcpServer &&server) noexcept;
    TcpServer(const TcpServer &server)=delete; //server can't copy
    TcpServer &operator=(const TcpServer &server)=delete;
    ~TcpServer();

public:
    bool Start();
    bool Stop();

    void SetConnCallback(ConnectionCallback conn_cb) { m_conn_cb = conn_cb; }
    void SetReadCallback(ReadCallback read_cb) { m_read_cb = read_cb; }
    void SetWriteCallback(WriteCallback write_cb) { m_wirte_cb = write_cb; }

private:
    bool Listen();
    void IOThreadProc();

private:
    int m_listen_port = -1;
    int m_listen_sock = -1;

    std::atomic<bool> m_loop_running = false;
    std::thread m_io_thread;

    std::map<int, epoll_event> m_connection_fd_map;

    ConnectionCallback m_conn_cb;
    ReadCallback m_read_cb;
    WriteCallback m_wirte_cb;
};

#endif