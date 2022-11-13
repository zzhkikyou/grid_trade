#pragma once

#include <string>
#include <sstream>
#include <chrono>
#include <iomanip>

// 获取本地时间 2022-1102-12:57:29 xxx:xxx:xxx
inline std::string GetLocalTime()
{
    std::time_t t = std::time(NULL);
    std::tm tm = *std::localtime(&t);
    std::stringstream szTime;

    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()) % 1000000;
    auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()) % 1000000000;

    szTime << std::put_time(&tm, "%Y-%m-%d-%H:%M:%S") << " "
           << std::setw(3) << std::setfill('0') << ms.count() << ":"
           << std::setw(3) << std::setfill('0') << us.count() % 1000 << ":"
           << std::setw(3) << std::setfill('0') << ns.count() % 1000;
    return szTime.str();
}

struct TimeDetails
{
    uint32_t Year;
    uint32_t month;
    uint32_t Day;
    uint32_t Hour;
    uint32_t Min;
    uint32_t Sec;
    uint32_t MSec;
    uint32_t USec;
    uint32_t NSec;
};

// 分解localtime
inline TimeDetails ResolveLocalTime(const std::string &szTime)
{
    TimeDetails Details;
    std::stringstream ss(szTime);
    std::string item;

    std::getline(ss, item, '-');
    Details.Year = std::stoi(item);
    std::getline(ss, item, '-');
    Details.month = std::stoi(item);
    std::getline(ss, item, '-');
    Details.Day = std::stoi(item);

    std::getline(ss, item, ':');
    Details.Hour = std::stoi(item);
    std::getline(ss, item, ':');
    Details.Min = std::stoi(item);
    std::getline(ss, item, ' ');
    Details.Sec = std::stoi(item);

    std::getline(ss, item, ':');
    Details.MSec = std::stoi(item);
    std::getline(ss, item, ':');
    Details.USec = std::stoi(item);
    std::getline(ss, item, ':');
    Details.NSec = std::stoi(item);

    return Details;
}

template <typename T>
inline uint64_t GetTs()
{
    auto now = std::chrono::system_clock::now();
    auto data = std::chrono::duration_cast<T>(now.time_since_epoch());
    return data.count();
}

inline const char *FormatDayInWeek(int32_t i)
{
    switch (i)
    {
    case 1:
        return "周一";
    case 2:
        return "周二";
    case 3:
        return "周三";
    case 4:
        return "周四";
    case 5:
        return "周五";
    case 6:
        return "周六";
    case 7:
        return "周日";
    default:
        return "";
    }
}

// 获取星期   1-7 星期一就是1
inline int32_t GetDayInWeek()
{
    std::time_t t = std::time(NULL);
    std::tm tm = *std::localtime(&t);
    std::stringstream szTime;
    szTime << std::put_time(&tm, "%u");
    return std::stoi(szTime.str());
}

inline void GetTimeInDay(int32_t &Hour, int32_t &Min)
{
    std::time_t t = std::time(NULL);
    std::tm tm = *std::localtime(&t);
    std::stringstream szTime, szTime2;
    szTime << std::put_time(&tm, "%H");
    Hour = std::stoi(szTime.str());
    szTime2 << std::put_time(&tm, "%M");
    Min = std::stoi(szTime2.str());
}

