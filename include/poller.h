#pragma once

#include <unistd.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <stdint.h>
#include <time.h>
#include <iostream>
#include <unordered_map>
#include <exception>
#include <functional>


struct PollUnit
{
    static const char *FormatPollType(int32_t EpollType)
    {
        switch (EpollType)
        {
        case EPOLLIN:
            return "EPOLLIN";
        case EPOLLPRI:
            return "EPOLLPRI";
        case EPOLLOUT:
            return "EPOLLOUT";
        case EPOLLRDNORM:
            return "EPOLLRDNORM";
        case EPOLLRDBAND:
            return "EPOLLRDBAND";
        case EPOLLWRNORM:
            return "EPOLLWRNORM";
        case EPOLLWRBAND:
            return "EPOLLWRBAND";
        case EPOLLMSG:
            return "EPOLLMSG";
        case EPOLLERR:
            return "EPOLLERR";
        case EPOLLHUP:
            return "EPOLLHUP";
        case EPOLLRDHUP:
            return "EPOLLRDHUP";
        case EPOLLWAKEUP:
            return "EPOLLWAKEUP";
        case EPOLLONESHOT:
            return "EPOLLONESHOT";
        default:
            return "Unknown";
        }
    }

    PollUnit(const std::function<void(int32_t, PollUnit *)> &_fnCallback, void *_lpArgs, int32_t _iFd, uint32_t _eEventType)
        : fnCallback_(_fnCallback), lpArgs_(_lpArgs), iFd_(_iFd), ePollType_(_eEventType) {}
    PollUnit &operator=(const PollUnit &Args)
    {
        fnCallback_ = Args.fnCallback_;
        lpArgs_ = Args.lpArgs_;
        iFd_ = Args.iFd_;
        ePollType_ = Args.ePollType_;
        eLastPollType_ = Args.eLastPollType_;
        return *this;
    }
    PollUnit(PollUnit &&Args)
    {
        operator=(Args);
    }
    PollUnit() = default;
    ~PollUnit() = default;
    void Release()
    {
        fnCallback_ = nullptr;
        lpArgs_ = nullptr;
        if (iFd_ != -1)
        {
            close(iFd_);
            iFd_ = -1;
        }
        ePollType_ = 0;
    }

    const char *FormatLastPollType()
    {
        return FormatPollType(eLastPollType_);
    }

    const char *FormatRegPollType()
    {
        return FormatPollType(ePollType_);
    }

    std::function<void(int32_t, PollUnit *)> fnCallback_;
    void *lpArgs_ = nullptr;
    int32_t iFd_ = -1;
    uint32_t ePollType_ = 0;
    uint32_t eLastPollType_ = 0; // 由 poll 赋值
};

class Poller
{
public:
    static const size_t MAX_EVENT_SIZE = 10240;

    Poller &operator=(const Poller &Args) = delete;
    Poller(Poller &&Args) = delete;

    Poller(uint32_t EpollSize = MAX_EVENT_SIZE)
    {
        if ((epollHandle_ = epoll_create(EpollSize)) == -1)
        {
            throw std::runtime_error("epoll_create");
        }
    }
    ~Poller()
    {
        DelAll();
        close(epollHandle_);
    }

    void Add(const PollUnit &rfUnit)
    {
        struct epoll_event EventData;
        EventData.data.fd = rfUnit.iFd_;
        // EventData.events = rfUnit.ePollType_ | EPOLLET;
        EventData.events = rfUnit.ePollType_;

        if (epoll_ctl(epollHandle_, EPOLL_CTL_ADD, rfUnit.iFd_, &EventData) == -1)
        {
            throw std::runtime_error("epoll_ctl");
        }
        events_[rfUnit.iFd_] = rfUnit;
    }

    void Del(uint32_t Handle)
    {
        if (events_.count(Handle) != 0)
        {
            epoll_ctl(epollHandle_, EPOLL_CTL_DEL, Handle, 0);
            events_.erase(Handle);
        }
    }

    void DelAll()
    {
        for (auto it = events_.begin(); it != events_.end();)
        {
            epoll_ctl(epollHandle_, EPOLL_CTL_DEL, it->first, 0);
            events_.erase(it++);
        }
    }

    size_t Size()
    {
        return events_.size();
    }

    size_t Proc(PollUnit *lpFns, size_t MaxPollSize)
    {
        thread_local struct epoll_event events[MAX_EVENT_SIZE];

        size_t uSize = MaxPollSize > MAX_EVENT_SIZE ? MAX_EVENT_SIZE : MaxPollSize;
        auto iCount = epoll_wait(epollHandle_, events, uSize, 500);
        size_t uCount = 0;

        for (int i = 0; i < iCount; ++i)
        {
            int32_t fd = events[i].data.fd;
            if (events_.count(fd) != 0)
            {
                auto &rfEvent = events_[fd];
                if (events[i].events & rfEvent.ePollType_ || events[i].events & EPOLLERR)
                {
                    rfEvent.eLastPollType_ = events[i].events;
                    lpFns[i] = rfEvent;
                    uCount++;
                }
            }
        }
        return uCount;
    }

private:
    int32_t epollHandle_ = -1;
    std::unordered_map<int32_t, PollUnit> events_;
};
