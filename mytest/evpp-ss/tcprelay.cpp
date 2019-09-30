//
// Created by berli on 2019/9/27.
//


// nc -l -p 3000 // 服务器接受      限速 nc -l -p 3000 | pv -L 1m > /dev/null
// tcprelay 127.0.0.1 3000 2000
// nc localhost 2000  // 客户端发送    pv /dev/zero | nc localhost 2000



#include "tunnel.h"
#include <evpp/current_thread.h>
#include <malloc.h>
#include <stdio.h>
#include <sys/resource.h>

evpp::EventLoop* g_eventLoop;
//InetAddress* g_serverAddr;
std::string g_serverAddr = "127.0.0.1:19099";
std::map<std::string, TunnelPtr> g_tunnels;


void onServerConnection(const evpp::TCPConnPtr& conn)
{
    LOG_DEBUG << (conn->IsConnected() ? "UP" : "DOWN");
    if (conn->IsConnected())
    {
        conn->SetTCPNoDelay(true);
        // conn->stopRead(); //  stopRead() 服务端可能还没建立起来  先不收数据
        TunnelPtr tunnel(new Tunnel(g_eventLoop, g_serverAddr, conn));
        tunnel->setup();
        tunnel->connect();
        g_tunnels[conn->name()] = tunnel; // 客户端
    }
    else
    {
        assert(g_tunnels.find(conn->name()) != g_tunnels.end());
        g_tunnels[conn->name()]->disconnect();
        g_tunnels.erase(conn->name());
    }
}

// 客户端消息过来
void onServerMessage(const evpp::TCPConnPtr& conn, evpp::Buffer* buf)
{
    LOG_DEBUG << buf->length();
    if (!conn->context().IsEmpty())
    {
        const evpp::TCPConnPtr clientConn = evpp::any_cast<const evpp::TCPConnPtr>(conn->context());// find the right server clientConn
        clientConn->Send(buf); //全部转发给真正的服务器
    }
}

void memstat()
{
    malloc_stats();
}

int main(int argc, char* argv[])
{
    if (argc < 4)
    {
        fprintf(stderr, "Usage: %s <host_ip> <port> <listen_port>\n", argv[0]);
    }
    else
    {
        LOG_INFO << "pid = " << getpid() << ", tid = " << current_thread::tid();
        {
            // set max virtual memory to 256MB.
            size_t kOneMB = 1024*1024;
            rlimit rl = { 256*kOneMB, 256*kOneMB };
            setrlimit(RLIMIT_AS, &rl); //限制一下内存使用
        }

        const char* ip = argv[1];
        uint16_t port = static_cast<uint16_t>(atoi(argv[2]));
        std::string serverAddr = std::string(ip) + ":" + std::to_string(port);
        g_serverAddr = serverAddr;

        uint16_t acceptPort = static_cast<uint16_t>(atoi(argv[3]));
        std::string listenAddr = std::string("0.0.0.0:") + std::to_string(acceptPort);

        evpp::EventLoop loop;
        g_eventLoop = &loop;

        loop.RunEvery(evpp::Duration(3), memstat); //每秒打印内存情况

        int thread_num = 4;

        evpp::TCPServer server(&loop, listenAddr, "TcpRelay", thread_num);
        server.SetConnectionCallback(onServerConnection);
        server.SetMessageCallback(onServerMessage);
        server.Init();
        server.Start();

        loop.Run();
    }
}