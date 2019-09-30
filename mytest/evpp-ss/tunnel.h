//
// Created by berli on 2019/9/27.
//

#ifndef SAFE_EVPP_TUNNEL_H
#define SAFE_EVPP_TUNNEL_H

#include <evpp/logging.h>
#include <evpp/event_loop.h>
#include <evpp/tcp_client.h>
#include <evpp/tcp_server.h>
#include <evpp/tcp_conn.h>

class Tunnel : public std::enable_shared_from_this<Tunnel>
{
public:
    Tunnel(evpp::EventLoop* loop, const std::string& serverAddr, const evpp::TCPConnPtr& serverConn)
            : client_(loop, serverAddr, serverConn->name()),
              serverConn_(serverConn)
    {
        LOG_INFO << "Tunnel " << serverConn->remote_addr()
                 << " <-> " << serverAddr;
    }

    ~Tunnel()
    {
        LOG_INFO << "~Tunnel";
    }

    void setup()
    {
        client_.SetConnectionCallback( std::bind(&Tunnel::onClientConnection, shared_from_this(), std::placeholders::_1));
        client_.SetMessageCallback( std::bind(&Tunnel::onClientMessage, shared_from_this(), std::placeholders::_1, std::placeholders::_2));
        serverConn_->SetHighWaterMarkCallback(
                std::bind(&Tunnel::onHighWaterMarkWeak, std::weak_ptr<Tunnel>(shared_from_this()), kServer, std::placeholders::_1, std::placeholders::_2),
                1024*1024);
    }

    void connect()
    {
        client_.Connect();//连接真正的服务器
    }

    void disconnect()
    {
        client_.Disconnect();
        // serverConn_.reset();
    }

private:
    void teardown()
    {
        client_.SetConnectionCallback(evpp::internal::DefaultConnectionCallback);
        client_.SetMessageCallback(evpp::internal::DefaultMessageCallback);
        if (serverConn_)
        {
            serverConn_->set_context(evpp::Any());
            serverConn_->Close();
        }
        clientConn_.reset();
    }

    //服务器连接成功
    void onClientConnection(const evpp::TCPConnPtr& conn)
    {
        LOG_DEBUG << (conn->IsConnected() ? "UP" : "DOWN");
        if (conn->IsConnected())
        {
            conn->SetTCPNoDelay(true);
            conn->SetHighWaterMarkCallback( std::bind(&Tunnel::onHighWaterMarkWeak, std::weak_ptr<Tunnel>(shared_from_this()), kClient, std::placeholders::_1, std::placeholders::_2),
                    1024*1024);

            serverConn_->set_context(evpp::Any(conn));
            // serverConn_ -> startRead();// 真正的服务器连接到来 可以开始启动读去客户端
            clientConn_ = conn;
            if (serverConn_->inputBuffer()->length() > 0)
            {
                conn->Send(serverConn_->inputBuffer());
            }
        }
        else
        {
            teardown();
        }
    }

    // 收到服务器消息
    void onClientMessage(const evpp::TCPConnPtr& conn,  evpp::Buffer* buf)
    {
        LOG_DEBUG << conn->name() << " " << buf->length();
        if (serverConn_)
        {
            serverConn_->Send(buf); //回传给客户端
        }
        else
        {
            buf->NextAll();
            abort();
        }
    }

    enum ServerClient
    {
        kServer, kClient
    };

    void onHighWaterMark(ServerClient which,  const evpp::TCPConnPtr& conn,  size_t bytesToSent)
    {
        LOG_INFO << (which == kServer ? "server" : "client")
                 << " onHighWaterMark " << conn->name()
                 << " bytes " << bytesToSent;

        if (which == kServer)// 客户端超高水位
        {
            if (serverConn_->outputBuffer()->length() > 0) // length() == readablebytes()
            {
                // clientConn_->HandleClose(); // 停止读
                serverConn_->SetWriteCompleteCallback(
                        std::bind(&Tunnel::onWriteCompleteWeak, std::weak_ptr<Tunnel>(shared_from_this()), kServer, std::placeholders::_1)
                );
            }
        }
        else
        {
            if (clientConn_->outputBuffer()->length() > 0)
            {
                // serverConn_->HandleClose(); // 停止读
                clientConn_->SetWriteCompleteCallback(
                        std::bind(&Tunnel::onWriteCompleteWeak, std::weak_ptr<Tunnel>(shared_from_this()), kClient, std::placeholders::_1)
                );
            }
        }
    }

    static void onHighWaterMarkWeak(const std::weak_ptr<Tunnel>& wkTunnel, ServerClient which, const evpp::TCPConnPtr& conn, size_t bytesToSent)
    {
        std::shared_ptr<Tunnel> tunnel = wkTunnel.lock();
        if (tunnel)
        {
            tunnel->onHighWaterMark(which, conn, bytesToSent);
        }
    }

    void onWriteComplete(ServerClient which, const evpp::TCPConnPtr& conn)
    {
        LOG_INFO << (which == kServer ? "server" : "client")
                 << " onWriteComplete " << conn->name();
        if (which == kServer) //都写回客户端了
        {
            // clientConn_->HandleRead(); // 可以开始读了服务器的消息
            serverConn_->SetWriteCompleteCallback(evpp::WriteCompleteCallback()); // 设置为空 不用每次发完数据都来回调
        }
        else // 都发给服务器了
        {
            // serverConn_->HandleRead(); // 继续读客户端的消息
            clientConn_->SetWriteCompleteCallback(evpp::WriteCompleteCallback()); //设置为空
        }
    }

    // 弱回调 防止循环引用
    static void onWriteCompleteWeak(const std::weak_ptr<Tunnel>& wkTunnel,  ServerClient which, const evpp::TCPConnPtr& conn)
    {
        std::shared_ptr<Tunnel> tunnel = wkTunnel.lock();
        if (tunnel)
        {
            tunnel->onWriteComplete(which, conn);
        }
    }

private:
    evpp::TCPClient client_; // 连接真正的服务器
    evpp::TCPConnPtr serverConn_;
    evpp::TCPConnPtr clientConn_;
};
typedef std::shared_ptr<Tunnel> TunnelPtr;

#endif //SAFE_EVPP_TUNNEL_H
