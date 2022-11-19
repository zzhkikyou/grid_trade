#pragma once
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/timerfd.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/eventfd.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <errno.h>
#include <iostream>
#include <unordered_map>
#include <exception>
#include <string>

enum class HandlerType : uint8_t
{
    Unknown,
    Timer,
    SocketAcceptor,
    SocketServer,
    SocketClient,
    Event,
};

class IHandler
{
public:
    virtual ~IHandler()
    {
        if (iFd_ != -1)
        {
            close(iFd_);
            iFd_ = -1;
        }
    }
    virtual int32_t GetFd() { return iFd_; };
    virtual void Close()
    {
        if (iFd_ != -1)
        {
            close(iFd_);
            iFd_ = -1;
        }
    }

    HandlerType GetType() { return eType_; }

    virtual ssize_t Write(const void *lpBuff, ssize_t Length) 
    {
        return write(iFd_, lpBuff, Length);
    }

    virtual ssize_t Read(void *lpBuff, ssize_t Length)
    {
        return read(iFd_, lpBuff, Length);
    }

    virtual void OnError()
    {
        Close();
    }

protected:
    int32_t iFd_ = -1;
    HandlerType eType_ = HandlerType::Unknown;
};

class TimerHandler : public IHandler
{
public:
    explicit TimerHandler(uint64_t uIntervalUs, bool bOnce = false)
    {
        eType_ = HandlerType::Timer;
        if ((iFd_ = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK)) == -1)
        {
            throw std::runtime_error("timerfd_create");
        }

        itimerspec Interval{};
        Interval.it_value.tv_nsec = uIntervalUs * 1000 % 1000000000;
        Interval.it_value.tv_sec = uIntervalUs / 1000000;
        if (!bOnce)
        {
            Interval.it_interval.tv_nsec = uIntervalUs * 1000 % 1000000000;
            Interval.it_interval.tv_sec = uIntervalUs / 1000000;
        }

        int32_t res = timerfd_settime(iFd_, 0, &Interval, 0);
        if (res == -1)
        {
            throw std::runtime_error("timerfd_settime");
        }
    }
    void ConsumeTimer()
    {
        uint64_t buf;
        if (Read(&buf, sizeof(uint64_t)) != sizeof(uint64_t))
        {
            perror("ConsumeTimer read fail");
        }
    }
};

class SocketHandler : public IHandler
{
public:
    SocketHandler()
    {
        CreateSocket();
    }

    void SetNonBlocking()
    {
        int32_t nFlags = 0;
        if ((nFlags = fcntl(iFd_, F_GETFL, 0)) < 0)
            throw std::runtime_error(std::to_string(iFd_) + " SetNonBlocking fcntl F_GETFL Fail");
        nFlags = nFlags | O_NONBLOCK;
        if (fcntl(iFd_, F_SETFL, nFlags) < 0)
            throw std::runtime_error(std::to_string(iFd_) + " SetNonBlocking fcntl F_SETFL Fail");
    }

    void SetBlocking()
    {
        int32_t nFlags = 0;
        if ((nFlags = fcntl(iFd_, F_GETFL, 0)) < 0)
            throw std::runtime_error(std::to_string(iFd_) + " SetNonBlocking fcntl F_GETFL Fail");
        nFlags = nFlags & (~O_NONBLOCK);
        if (fcntl(iFd_, F_SETFL, nFlags) < 0)
            throw std::runtime_error(std::to_string(iFd_) + " SetNonBlocking fcntl F_SETFL Fail");
    }

    void SetReuseAddr()
    {
        int32_t Opt = 1;
        if (setsockopt(iFd_, SOL_SOCKET, SO_REUSEADDR, (const char *)&Opt, sizeof(Opt)))
        {
            throw std::runtime_error("SetReuseAddr setsockopt");
        }
    }
    void SetReusePort()
    {
        int32_t Opt = 1;
        if (setsockopt(iFd_, SOL_SOCKET, SO_REUSEPORT, (const char *)&Opt, sizeof(Opt)))
        {
            throw std::runtime_error("SO_REUSEPORT setsockopt");
        }
    }

    void CreateSocket()
    {
        if (iFd_ != -1)
        {
            Close();
        }

        if ((iFd_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
        {
            throw std::runtime_error("socket");
        }
    }
};

class SocketServer : public SocketHandler
{
public:
    ~SocketServer() {}

    SocketServer(int32_t _iFd, const std::string &_Ip, uint16_t _Port) 
    : Ip_(_Ip), Port_(_Port)
    {
        eType_ = HandlerType::SocketServer;
        iFd_ = _iFd;
        SetNonBlocking();
    }

    std::string GetPeerIp()
    {
        return Ip_;
    }

    uint16_t GetPeerPort()
    {
        return Port_;
    }

private:
    std::string Ip_;
    uint16_t Port_;
};

class SocketClient : public SocketHandler
{
public:
    ~SocketClient() {}

    SocketClient(const std::string &_Ip, uint16_t _Port)
    {
        SetNonBlocking();

        eType_ = HandlerType::SocketClient;
        sockserver_.sin_family = AF_INET;
        sockserver_.sin_port = htons(_Port);
        inet_pton(AF_INET, _Ip.c_str(), &sockserver_.sin_addr);

        Ip_ = _Ip;
        Port_ = _Port;
    }

    int32_t Connect()
    {
        if (iFd_ == -1)
        {
            CreateSocket();
            SetNonBlocking();
        }

        if (connect(iFd_, (sockaddr *)&sockserver_, (socklen_t)sizeof(sockaddr_in)) != 0)
        {
            if (errno != EINPROGRESS)
            {
                return -1;
            }
        }
        return 0;
    }

private:
    struct sockaddr_in sockserver_;
    std::string Ip_;
    uint16_t Port_ = 0;
};

class SocketAcceptor : public SocketHandler
{
public:
    SocketAcceptor(const std::string &Ip_, uint16_t uPort)
    {
        SetNonBlocking();
        SetReuseAddr();
        SetReusePort();

        eType_ = HandlerType::SocketAcceptor;
        struct sockaddr_in sockaddr;
        sockaddr.sin_family = AF_INET;
        if (Ip_ == "")
        {
            sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);
        }
        else
        {
            sockaddr.sin_addr.s_addr = inet_addr(Ip_.c_str());
        }
        sockaddr.sin_port = htons(uPort);
        if (bind(iFd_, (struct sockaddr *)&sockaddr, sizeof(sockaddr)) == -1)
        {
            throw std::runtime_error("SocketAcceptor bind");
        }
        
        if (listen(iFd_, 1024) == -1)
        {
            throw std::runtime_error("SocketAcceptor listen");
        }
    }

    SocketServer *Accept()
    {
        struct sockaddr_in caddr = {0};
        socklen_t nsize = (socklen_t)sizeof(caddr);
        int cfd = accept(iFd_, (struct sockaddr *)&caddr, &nsize);
        return new SocketServer(cfd, inet_ntoa(caddr.sin_addr), ntohs(caddr.sin_port));
    }
};

class EventHandler : public IHandler
{
public:
    EventHandler()
    {
        eType_ = HandlerType::Event;

        iFd_ = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    }

    void Consume()
    {
        uint64_t one = 1;
        ssize_t n = Read(&one, sizeof(one));
        if (n != sizeof(one))
        {
            perror("Consume read fail");
        }
    }

    void Post()
    {
        uint64_t one = 1;
        ssize_t n = Write(&one, sizeof(one));
        if (n != sizeof(one))
        {
            perror("Post write fail");
        }
    }
};

