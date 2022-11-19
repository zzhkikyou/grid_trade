#pragma once

#include <vector>
#include <functional>
#include <atomic>
#include <list>

#include "Log.h"
#include "allocator.h"
#include "handler.h"
#include "poller_loop.h"

class ClientConnection;
class ServerConnection;

// 回调函数
enum class ControlType
{
    Connect,
    Disconnect,
};


class ServerConnection
{
    friend class Server;

public:
    ServerConnection() = default;
    explicit ServerConnection(SocketServer *_lpHandler) : lpHandler_(_lpHandler) {}
    ~ServerConnection()
    {
        delete lpHandler_;
    }

    void Send(const void *lpBuff, uint32_t Length)
    {
        std::unique_lock<std::recursive_mutex> lock(SendMutex_);
        memcpy(SendAlloc_.AllocMemContinuous(Length), lpBuff, Length);
        SendAlloc_.ConfirmUsage(Length);
    }

    void Disconnect()
    {
        fnControlCallback_(lpHandler_->GetFd(), ControlType::Disconnect, this);
    }

    bool IsConnected()
    {
        return bConnected_;
    }

    void SetHandler(SocketServer *_lpHandler)
    {
        lpHandler_ = _lpHandler;
    }

    SocketServer *GetHandler()
    {
        return lpHandler_;
    }

private:
    SocketServer *lpHandler_ = nullptr;

    allocator RecvAlloc_;

    std::recursive_mutex SendMutex_;
    allocator SendAlloc_;

    volatile bool bConnected_ = false;
    std::function<int32_t(int32_t, ControlType, ServerConnection *)> fnControlCallback_;
    void *lpArgs_ = nullptr;
};

class Server
{
public:
    Server(const std::string &_Ip, uint16_t _Port, uint32_t _MaxConnectionSize = 10240) : AcceptorH_(_Ip, _Port)
    {
        MaxConnectionSize_ = _MaxConnectionSize;
    }

    ~Server()
    {
    }

    void Start()
    {
        bTerminated_ = false;
        PollUnit AcceptUnit(std::bind(&Server::OnProcessSelf, this, std::placeholders::_1, std::placeholders::_2), this, AcceptorH_.GetFd(), EPOLLIN);
        Loop_.Add(AcceptUnit);
        Loop_.Start("Server");
    }

    void Stop()
    {
        bTerminated_ = true;
        Loop_.Stop();
    }
    void SetfnOnRecvRawData(const std::function<uint32_t(ServerConnection *, void *, uint32_t)> &fn) { fnOnRecvRawData_ = fn; };
    void SetfnOnMessage(const std::function<void(ServerConnection *, void *, uint32_t)> &fn) { fnOnMessage_ = fn; }
    void SetfnOnAccepted(const std::function<void(ServerConnection *)> &fn) { fnOnAccepted_ = fn; }
    void SetfnOnDisconnected(const std::function<void(ServerConnection *)> &fn) { fnOnDisconnected_ = fn; };
    
    uint32_t GetOnlineSize()
    {
        std::unique_lock<std::recursive_mutex> lock(Loop_.GetMutex());
        return Connections_.size();
    }

private:

    int32_t OnControlSelf(int32_t fd, ControlType Type, ServerConnection *lpConnect)
    {
        std::unique_lock<std::recursive_mutex> lock(Loop_.GetMutex());
        {
            LOG_INFO("主动断开");
            if (!lpConnect->IsConnected())
            {
                LOG_WARN("连接已经断开, 无需重新断开");
                return 0;
            }
            OnDisconnected(lpConnect);
        }
        return 0;
    }

    void OnProcessSelf(int32_t fd, PollUnit *lpUnit)
    {
        if (fd == AcceptorH_.GetFd())
        {
            if (lpUnit->eLastPollType_ & EPOLLIN)
            {
                OnAccept(fd, lpUnit);
            }
        }
        else
        {
            std::unique_lock<std::recursive_mutex> lock(Loop_.GetMutex());
            ServerConnection &Conn = Connections_[fd];
            if (lpUnit->eLastPollType_ & EPOLLERR)
            {
                int error = 0;
                socklen_t length = sizeof(error);
                if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &length) < 0)
                {
                    OnError(fd, lpUnit, &Conn);
                    return;
                }
            }
            else if (lpUnit->eLastPollType_ & EPOLLHUP)
            {
                OnDisconnected(&Conn);
                return;
            }
            else
            {
                if (!Conn.IsConnected())
                {
                    OnConnected(&Conn);
                }

                if (lpUnit->eLastPollType_ & EPOLLIN)
                {
                    if (!OnRecv(fd, lpUnit, &Conn))
                    {
                        // LOG_WARN("OnRecv FAIL");
                        OnDisconnected(&Conn);
                        return;
                    }
                }
                else if (lpUnit->eLastPollType_ & EPOLLOUT)
                {
                    if (!OnSend(fd, lpUnit, &Conn))
                    {
                        // LOG_WARN("OnSend FAIL");
                        OnDisconnected(&Conn);
                        return;
                    }
                }
            }
        }
    }

    void OnAccept(int32_t fd, PollUnit *lpUnit)
    {
        SocketServer *lpSock = AcceptorH_.Accept();
        LOG_WARN("客户端接入事件 fd:%d, 新 fd:%d, %s:%d", fd, lpSock->GetFd(), lpSock->GetPeerIp().c_str(), lpSock->GetPeerPort());
        auto &rfConnection = Connections_[lpSock->GetFd()];
        rfConnection.SetHandler(lpSock);
        rfConnection.fnControlCallback_ = std::bind(&Server::OnControlSelf, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
        rfConnection.lpArgs_ = this;
        PollUnit Unit(std::bind(&Server::OnProcessSelf, this, std::placeholders::_1, std::placeholders::_2), this, lpSock->GetFd(), EPOLLIN | EPOLLOUT);
        Loop_.Add(Unit);
    }

    bool OnRecv(int32_t fd, PollUnit *lpUnit, ServerConnection *lpConn)
    {
        LOG_TRACE("接收事件 fd:%d, %s:%d", fd, lpConn->GetHandler()->GetPeerIp().c_str(), lpConn->GetHandler()->GetPeerPort());

        while (!bTerminated_)
        {
            ssize_t iSize = recv(fd, lpConn->RecvAlloc_.AllocMemContinuous(1024), 1024, 0);
            if (iSize > 0)
            {
                lpConn->RecvAlloc_.ConfirmUsage(iSize);
            }
            else if (iSize == 0)
            {
                return false;
            }
            else if (errno != EAGAIN && errno != EWOULDBLOCK)
            {
                return false;
            }
            else
            {
                break;
            }
        }

        if (lpConn->RecvAlloc_.GetUsage() == 0)
        {
            return 0;
        }

        try
        {
            if (fnOnRecvRawData_)
            {
                uint32_t uLength = 0;
                while (!bTerminated_ && lpConn->IsConnected() &&
                       ((uLength = fnOnRecvRawData_(lpConn, lpConn->RecvAlloc_.GetBuffer(), lpConn->RecvAlloc_.GetUsage())) <= lpConn->RecvAlloc_.GetUsage()))
                {
                    if (uLength == 0)
                    {
                        return true;
                    }

                    if (fnOnMessage_)
                    {
                        fnOnMessage_(lpConn, lpConn->RecvAlloc_.GetBuffer(), uLength);
                    }
                    lpConn->RecvAlloc_.RemoveUsage(uLength);
                }
            }
            else
            {
                if (fnOnMessage_)
                {
                    fnOnMessage_(lpConn, lpConn->RecvAlloc_.GetBuffer(), lpConn->RecvAlloc_.GetUsage());
                    lpConn->RecvAlloc_.RemoveUsage(lpConn->RecvAlloc_.GetUsage());
                }
            }
        }
        catch (const std::exception &e)
        {
            std::cerr << e.what() << '\n';
            return false;
        }

        return true;
    }

    bool OnSend(int32_t fd, PollUnit *lpUnit, ServerConnection *lpConn)
    {
        LOG_TRACE("发送事件 fd:%d, %s:%d", fd, lpConn->GetHandler()->GetPeerIp().c_str(), lpConn->GetHandler()->GetPeerPort());

        std::unique_lock<std::recursive_mutex> lock(lpConn->SendMutex_);
        while (!bTerminated_ && lpConn->SendAlloc_.GetUsage())
        {
            ssize_t iSize = send(fd, lpConn->SendAlloc_.GetBuffer(), lpConn->SendAlloc_.GetUsage(), MSG_NOSIGNAL);
            if (iSize >= 0)
            {
                lpConn->SendAlloc_.RemoveUsage(iSize);
            }
            else if (errno == EINTR)
            {
                continue;
            }
            else if (errno != EAGAIN && errno != EWOULDBLOCK)
            {
                return false;
            }
            else
            {
                break;
            }
        }

        return true;
    }

    void OnError(int32_t fd, PollUnit *lpUnit, ServerConnection *lpConn)
    {
        LOG_WARN("错误事件 fd:%d, %s:%d", fd, lpConn->GetHandler()->GetPeerIp().c_str(), lpConn->GetHandler()->GetPeerPort());
        OnDisconnected(lpConn);
    }

    void OnConnected(ServerConnection *lpConn)
    {
        LOG_WARN("连接事件 EVENT fd:%d, %s:%d", lpConn->GetHandler()->GetFd(), lpConn->GetHandler()->GetPeerIp().c_str(), lpConn->GetHandler()->GetPeerPort());
        lpConn->bConnected_ = true;
        if (fnOnAccepted_)
        {
            fnOnAccepted_(lpConn);
        }
    }

    void OnDisconnected(ServerConnection *lpConn)
    {
        LOG_WARN("断开事件 fd:%d, %s:%d", lpConn->GetHandler()->GetFd(), lpConn->GetHandler()->GetPeerIp().c_str(), lpConn->GetHandler()->GetPeerPort());
        lpConn->bConnected_ = false;
        if (fnOnDisconnected_)
        {
            fnOnDisconnected_(lpConn);
        }
        Loop_.Del(lpConn->GetHandler()->GetFd());
        lpConn->GetHandler()->Close();
        Connections_.erase(lpConn->GetHandler()->GetFd());
    }

private:
    SocketAcceptor AcceptorH_;

    PollLoop Loop_{4096};

    std::unordered_map<int32_t, ServerConnection> Connections_;
    uint32_t MaxConnectionSize_ = 10240;

    volatile bool bTerminated_ = false;

private:
    std::function<void(ServerConnection *, void *, uint32_t)> fnOnMessage_;
    std::function<uint32_t(ServerConnection *, void *, uint32_t)> fnOnRecvRawData_; // 返回需要的包长度，未知返回0，非法包需要抛异常

    std::function<void(ServerConnection *)> fnOnAccepted_;
    std::function<void(ServerConnection *)> fnOnDisconnected_;
};

class ClientConnection
{
    friend class Client;

public:
    ClientConnection() = default;
    explicit ClientConnection(SocketClient *_lpHandler) : lpHandler_(_lpHandler) {}
    ~ClientConnection()
    {
        delete lpHandler_;
    }

    void Send(const void *lpBuff, uint32_t Length)
    {
        std::unique_lock<std::recursive_mutex> lock(SendMutex_);
        memcpy(SendAlloc_.AllocMemContinuous(Length), lpBuff, Length);
        SendAlloc_.ConfirmUsage(Length);
    }

    void Disconnect()
    {
        fnControlCallback_(lpHandler_->GetFd(), ControlType::Disconnect, this);
    }

    int32_t Connect()
    {
        return fnControlCallback_(lpHandler_->GetFd(), ControlType::Connect, this);
    }

    bool IsConnected()
    {
        return bConnected_;
    }

    SocketClient *GetHandler()
    {
        return lpHandler_;
    }

    void SetHandler(SocketClient *_lpHandler)
    {
        lpHandler_ = _lpHandler;
    }

private:
    SocketClient *lpHandler_ = nullptr;

    allocator RecvAlloc_;

    std::recursive_mutex SendMutex_;
    allocator SendAlloc_;

    volatile bool bConnected_ = false;

private:
    std::function<void(void *, uint32_t)> fnOnMessage_;
    std::function<uint32_t(void *, uint32_t)> fnOnRecvRawData_; // 返回需要的包长度，未知返回0，非法包需要抛异常

    std::function<void()> fnOnConnected_;
    std::function<void()> fnOnDisconnected_;
    std::function<int32_t(int32_t, ControlType Type, ClientConnection *)> fnControlCallback_;
    void *lpArgs_ = nullptr;
};

class Client
{
    friend class ClientConnection;

public:
    Client()
    {
    }
    ~Client()
    {
        std::unique_lock<std::recursive_mutex> lock(Loop_.GetMutex());
        for (auto &it : Connections_)
        {
            delete it.second;
        }
    }

    ClientConnection *NewConnection(const std::string &_Ip,
                                    uint16_t _Port,
                                    std::function<void(void *, uint32_t)> _fnOnMessage = nullptr,
                                    std::function<uint32_t(void *, uint32_t)> _fnOnRecvRawData = nullptr,
                                    std::function<void()> _fnOnConnected = nullptr,
                                    std::function<void()> _fnOnDisconnected = nullptr)
    {
        std::unique_lock<std::recursive_mutex> lock(Loop_.GetMutex());
        SocketClient *lpSock = new SocketClient(_Ip, _Port);
        auto lpConnection = new ClientConnection(lpSock);
        lpConnection->fnOnMessage_ = _fnOnMessage;
        lpConnection->fnOnRecvRawData_ = _fnOnRecvRawData;
        lpConnection->fnOnConnected_ = _fnOnConnected;
        lpConnection->fnOnDisconnected_ = _fnOnDisconnected;
        lpConnection->fnControlCallback_ = std::bind(&Client::OnControlSelf, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
        lpConnection->lpArgs_ = this;
        Connections_[lpSock->GetFd()] = lpConnection;
        PollUnit Unit(std::bind(&Client::OnProcessSelf, this, std::placeholders::_1, std::placeholders::_2), this, lpSock->GetFd(), EPOLLIN | EPOLLOUT);
        Loop_.Add(Unit);
        return lpConnection;
    }

    void DeleteConnection(ClientConnection *lpConn)
    {
        std::unique_lock<std::recursive_mutex> lock(Loop_.GetMutex());
        auto fd = lpConn->GetHandler()->GetFd();
        lpConn->Disconnect();
        if (fd != -1)
        {
            Connections_.erase(fd);
        }
        delete lpConn;
    }

    void Start()
    {
        bTerminated_ = false;
        Loop_.Start("Client");
    }

    void Stop()
    {
        bTerminated_ = true;
        Loop_.Stop();
    }

private:
    int32_t OnControlSelf(int32_t fd, ControlType Type, ClientConnection *lpConnect)
    {
        std::unique_lock<std::recursive_mutex> lock(Loop_.GetMutex());
        if (Type == ControlType::Connect)
        {
            LOG_INFO("尝试连接, fd %d", fd);
            if (lpConnect->IsConnected())
            {
                LOG_WARN("连接已经建立, 无需重新建立, fd %d", fd);
                return 0;
            }
            Connections_.erase(lpConnect->GetHandler()->GetFd());
            lpConnect->GetHandler()->CreateSocket();
            Connections_[lpConnect->GetHandler()->GetFd()] = lpConnect;
            lpConnect->GetHandler()->SetNonBlocking();
            PollUnit Unit(std::bind(&Client::OnProcessSelf, this, std::placeholders::_1, std::placeholders::_2), this, lpConnect->GetHandler()->GetFd(), EPOLLIN | EPOLLOUT);
            Loop_.Add(Unit);
            return lpConnect->GetHandler()->Connect();
        }
        else
        {
            LOG_INFO("主动断开, fd %d", fd);
            if (!lpConnect->IsConnected() && (lpConnect->GetHandler()->GetFd() == -1))
            {
                LOG_WARN("连接已经断开, 无需重新断开, fd %d", fd);
                return 0;
            }
            OnDisconnected(lpConnect);
        }
        return 0;
    }

    void OnProcessSelf(int32_t fd, PollUnit *lpUnit)
    {
        std::unique_lock<std::recursive_mutex> lock(Loop_.GetMutex());
        if (Connections_.count(fd) == 0)
        {
            LOG_FATAL("未知的 fd(%d) 事件!!!!", fd);
            abort();
        }

        ClientConnection *Conn = Connections_[fd];
        if (lpUnit->eLastPollType_ & EPOLLERR)
        {
            int error = 0;
            socklen_t length = sizeof(error);
            if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &length) < 0)
            {
                OnError(fd, lpUnit, Conn);
                return;
            }
        }
        else
        {
            if (!Conn->IsConnected())
            {
                OnConnected(Conn);
            }

            if (lpUnit->eLastPollType_ & EPOLLIN)
            {
                if (!OnRecv(fd, lpUnit, Conn))
                {
                    // LOG_WARN("OnRecv FAIL");
                    OnDisconnected(Conn);
                    return;
                }
            }
            else if (lpUnit->eLastPollType_ & EPOLLOUT)
            {
                if (!OnSend(fd, lpUnit, Conn))
                {
                    // LOG_WARN("OnSend FAIL");
                    OnDisconnected(Conn);
                    return;
                }
            }
        }
    }

    bool OnRecv(int32_t fd, PollUnit *lpUnit, ClientConnection *lpConn)
    {
        while (!bTerminated_)
        {
            ssize_t iSize = recv(fd, lpConn->RecvAlloc_.AllocMemContinuous(1024), 1024, 0);
            if (iSize > 0)
            {
                lpConn->RecvAlloc_.ConfirmUsage(iSize);
            }
            else if (iSize == 0)
            {
                return false;
            }
            else if (errno != EAGAIN && errno != EWOULDBLOCK)
            {
                return false;
            }
            else
            {
                break;
            }
        }

        if (lpConn->RecvAlloc_.GetUsage() == 0)
        {
            return 0;
        }

        try
        {
            if (lpConn->fnOnRecvRawData_)
            {
                uint32_t uLength = 0;
                while (!bTerminated_ && lpConn->IsConnected() &&
                       ((uLength = lpConn->fnOnRecvRawData_(lpConn->RecvAlloc_.GetBuffer(), lpConn->RecvAlloc_.GetUsage())) <= lpConn->RecvAlloc_.GetUsage()))
                {
                    if (uLength == 0)
                    {
                        return true;
                    }

                    if (lpConn->fnOnMessage_)
                    {
                        lpConn->fnOnMessage_(lpConn->RecvAlloc_.GetBuffer(), uLength);
                    }
                    lpConn->RecvAlloc_.RemoveUsage(uLength);
                }
            }
            else
            {
                if (lpConn->fnOnMessage_)
                {
                    lpConn->fnOnMessage_(lpConn->RecvAlloc_.GetBuffer(), lpConn->RecvAlloc_.GetUsage());
                    lpConn->RecvAlloc_.RemoveUsage(lpConn->RecvAlloc_.GetUsage());
                }
            }
        }
        catch (const std::exception &e)
        {
            std::cerr << e.what() << '\n';
            return false;
        }

        return true;
    }

    bool OnSend(int32_t fd, PollUnit *lpUnit, ClientConnection *lpConn)
    {
        LOG_TRACE("发送事件 fd:%d", fd);

        std::unique_lock<std::recursive_mutex> lock(lpConn->SendMutex_);
        while (!bTerminated_ && lpConn->SendAlloc_.GetUsage())
        {
            ssize_t iSize = send(fd, lpConn->SendAlloc_.GetBuffer(), lpConn->SendAlloc_.GetUsage(), MSG_NOSIGNAL);
            if (iSize >= 0)
            {
                lpConn->SendAlloc_.RemoveUsage(iSize);
            }
            else if (errno == EINTR)
            {
                continue;
            }
            else if (errno != EAGAIN && errno != EWOULDBLOCK)
            {
                return false;
            }
            else
            {
                break;
            }
        }

        return true;
    }

    void OnError(int32_t fd, PollUnit *lpUnit, ClientConnection *lpConn)
    {
        LOG_WARN("错误事件 fd:%d", fd);
        OnDisconnected(lpConn);
    }

    void OnConnected(ClientConnection *lpConn)
    {
        LOG_WARN("连接事件 fd:%d", lpConn->GetHandler()->GetFd());
        lpConn->bConnected_ = true;
        if (lpConn->fnOnConnected_)
        {
            lpConn->fnOnConnected_();
        }
    }

    void OnDisconnected(ClientConnection *lpConn)
    {
        LOG_WARN("断开事件 fd:%d", lpConn->GetHandler()->GetFd());
        lpConn->bConnected_ = false;
        if (lpConn->fnOnDisconnected_)
        {
            lpConn->fnOnDisconnected_();
        }
        Loop_.Del(lpConn->GetHandler()->GetFd());
        lpConn->GetHandler()->Close();
    }

private:
    volatile bool bTerminated_ = false;
    PollLoop Loop_{1024};

    std::unordered_map<int32_t, ClientConnection *> Connections_;
    uint32_t MaxConnectionSize_ = 10240;
};
