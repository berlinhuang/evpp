//
// Created by berli on 2019/9/30.
//

#include "tunnel.h"

#include <evpp/endian.h>
#include <evpp/current_thread.h>
#include <stdio.h>
#include <netdb.h>


evpp::EventLoop* g_eventLoop;
std::map<std::string, TunnelPtr> g_tunnels;

void onServerConnection(const evpp::TCPConnPtr& conn)
{
    LOG_DEBUG << conn->name() << (conn->IsConnected() ? " UP" : " DOWN");
    if (conn->IsConnected())
    {
        conn->SetTCPNoDelay(true);
    }
    else
    {
        std::map<std::string, TunnelPtr>::iterator it = g_tunnels.find(conn->name());
        if (it != g_tunnels.end())
        {
            it->second->disconnect();
            g_tunnels.erase(it);
        }
    }
}

void onServerMessage(const evpp::TCPConnPtr& conn, evpp::Buffer* buf)
{
    LOG_DEBUG << conn->name() << " " << buf->length();
    if (g_tunnels.find(conn->name()) == g_tunnels.end())
    {
        if (buf->length() > 128)
        {
            conn->shutdown();
        }
        else if (buf->length() > 8)
        {
            const char* begin = buf->data() + 8;
            const char* end = buf->data() + buf->length();
            const char* where = std::find(begin, end, '\0');
            if (where != end)
            {
                char ver = buf->data()[0];
                char cmd = buf->data()[1];
                const void* port = buf->data() + 2;
                const void* ip = buf->data() + 4;

                sockaddr_in addr;
                bzero(&addr, sizeof addr);
                addr.sin_family = AF_INET;
                addr.sin_port = *static_cast<const in_port_t*>(port);
                addr.sin_addr.s_addr = *static_cast<const uint32_t*>(ip);

                bool socks4a = sockets::networkToHost32(addr.sin_addr.s_addr) < 256;
                bool okay = false;
                if (socks4a)
                {
                    const char* endOfHostName = std::find(where+1, end, '\0');
                    if (endOfHostName != end)
                    {
                        std::string hostname = where+1;
                        where = endOfHostName;
                        LOG_INFO << "Socks4a host name " << hostname;
                        InetAddress tmp;
                        if (InetAddress::resolve(hostname, &tmp))
                        {
                            addr.sin_addr.s_addr = tmp.ipNetEndian();
                            okay = true;
                        }
                    }
                    else
                    {
                        return;
                    }
                }
                else
                {
                    okay = true;
                }

                InetAddress serverAddr(addr);
                if (ver == 4 && cmd == 1 && okay)
                {
                    TunnelPtr tunnel(new Tunnel(g_eventLoop, serverAddr, conn));
                    tunnel->setup();
                    tunnel->connect();
                    g_tunnels[conn->name()] = tunnel;
                    buf->retrieveUntil(where+1);
                    char response[] = "\000\x5aUVWXYZ";
                    memcpy(response+2, &addr.sin_port, 2);
                    memcpy(response+4, &addr.sin_addr.s_addr, 4);
                    conn->Send(response, 8);
                }
                else
                {
                    char response[] = "\000\x5bUVWXYZ";
                    conn->Send(response, 8);
                    conn->shutdown();
                }
            }
        }
    }
    else if (!conn->context().IsEmpty())
    {
        const evpp::TCPConnPtr clientConn = evpp::any_cast<const evpp::TCPConnPtr>(conn->context());
        clientConn->Send(buf);
    }
}

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <listen_port>\n", argv[0]);
    }
    else
    {
        LOG_INFO << "pid = " << getpid() << ", tid = " << current_thread::tid();

        uint16_t port = static_cast<uint16_t>(atoi(argv[1]));
        std::string listenAddr = std::string("0.0.0.0:") + std::to_string(port);

        evpp::EventLoop loop;
        g_eventLoop = &loop;

        evpp::TCPServer server(&loop, listenAddr, "Socks4", 4);

        server.SetConnectionCallback(onServerConnection);
        server.SetMessageCallback(onServerMessage);
        server.Init();
        server.Start();

        loop.Run();
    }
}