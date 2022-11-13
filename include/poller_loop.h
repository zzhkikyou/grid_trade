#pragma once
#include "Log.h"
#include "poller.h"
#include <thread>
#include <mutex>
#include <functional>
#include <atomic>

class PollLoop
{
public:
    PollLoop(uint32_t EventSize = 1024, const std::function<void()> &OnNotify = nullptr, void *lpArgs = nullptr)
        : poller_(EventSize), OnNotifyCallback_(OnNotify), lpArgs_(lpArgs) {}
    ~PollLoop() { Stop(); }

    void Add(const PollUnit &rfUnit)
    {
        std::unique_lock<std::recursive_mutex> lock(mutex_);
        return poller_.Add(rfUnit);
    }

    void Del(uint32_t Handle)
    {
        std::unique_lock<std::recursive_mutex> lock(mutex_);
        return poller_.Del(Handle);
    }

    void DelAll()
    {
        std::unique_lock<std::recursive_mutex> lock(mutex_);
        return poller_.DelAll();
    }

    void Notify()
    {
        std::unique_lock<std::recursive_mutex> lock(mutex_);
        notify_ = true;
    }

    std::recursive_mutex &GetMutex()
    {
        return mutex_;
    }

    size_t Size()
    {
        return poller_.Size();
    }

    void Start(const std::string &ThreadName = "")
    {

        OwnThreadCheck();
        if (!bRunning_)
        {
            bTerminated_ = false;
            thread_ = std::thread(&PollLoop::Run, this, ThreadName);
            while (!bRunning_)
                ;
        }
    }

    void Stop()
    {
        OwnThreadCheck();
        if (bRunning_)
        {
            bTerminated_ = true;
            while (bRunning_)
                ;
            if (thread_.joinable())
            {
                thread_.join();
            }
        }
    }

private:
    void Run(const std::string &ThreadName = "")
    {
        if (!ThreadName.empty())
        {
            REGTHREADINFO(ThreadName);
        }

        bRunning_ = true;
        PollUnit Units[Poller::MAX_EVENT_SIZE];
        while (!bTerminated_)
        {
            try
            {
                std::unique_lock<std::recursive_mutex> lock(mutex_);
                if (notify_)
                {
                    notify_ = false;
                    if (OnNotifyCallback_)
                    {
                        OnNotifyCallback_();
                    }
                }

                size_t Count = poller_.Proc(Units, Poller::MAX_EVENT_SIZE);
                for (size_t i = 0; i < Count; i++)
                {
                    Units[i].fnCallback_(Units[i].iFd_, &Units[i]);
                }
            }
            catch (const std::exception &e)
            {
                std::cerr << e.what() << '\n';
            }
            std::this_thread::yield();
        }
        bRunning_ = false;
    }

private:
    void OwnThreadCheck()
    {
        if (std::this_thread::get_id() == thread_.get_id())
        {
            throw std::runtime_error("不可以在本线程操作");
        }
    }

private:
    Poller poller_;
    std::thread thread_;
    std::recursive_mutex mutex_;
    std::atomic<bool> notify_{false};
    volatile bool bTerminated_ = true;
    volatile bool bRunning_ = false;

    std::function<void()> OnNotifyCallback_;
    void *lpArgs_ = nullptr;
};
