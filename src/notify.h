#ifndef __NOTIFY_H_
#define __NOTIFY_H_

#include "common.h"

class notify
{
public:
    notify() = default;
    ~notify() = default;

    void notify_zzh(uint64_t identify, const std::string &szTitle, const std::string &szMsg);

private:
    uint64_t Lastidentify = 0;
};

#endif