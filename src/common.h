#ifndef _COMMON_H_
#define _COMMON_H_

#include <memory>
#include <fstream>
#include <netdb.h>
#include <arpa/inet.h>
#include <signal.h>

#include "daemon.h"
#include "encode_convert.h"
#include "httpparser/httpresponseparser.h"
#include "tcp.h"
#include "httpparser/httprequestparser.h"
#include "http_assemble.h"
#include "Log.h"
#include "grid_trade.h"
#include "http_client.h"
#include "http_server.h"
#include "format_time.h"
#include "postion.h"
#include "persistence.h"
#include "shell_exec.h"
#include "notify.h"
#include "cJSON.h"

extern volatile bool g_bTerminated;
inline bool IsTerminated()
{
    return g_bTerminated;
}

/** 策略单例 */
inline ITradeStrategy *GetGridTrade()
{
    static ITradeStrategy *lp1 = new GridTrade;
    return lp1;
}

/** 仓位单例 */
inline PostionMgr *GePostionMgr()
{
    static PostionMgr *lp2 = new PostionMgr;
    return lp2;
}

/** 写持久化单例 */
inline PersistenceWriter *GePersistenceWriter()
{
    static PersistenceWriter *lp3 = new PersistenceWriter;
    return lp3;
}

/** 读持久化单例 */
inline PersistenceReader *GePersistenceReader()
{
    static PersistenceReader *lp4 = new PersistenceReader;
    return lp4;
}

/** 写历史股价记录单例 */
inline PersistenceWriter *GePersistenceStockWriter()
{
    static PersistenceWriter *lp5 = new PersistenceWriter;
    return lp5;
}

/** 读历史股价记录单例 */
inline PersistenceReader *GePersistenceStockReader()
{
    static PersistenceReader *lp6 = new PersistenceReader;
    return lp6;
}

extern volatile bool g_bTest;

// 判断当前是否为交易时间
inline bool IsDealTime(int32_t &Hour, int32_t &Min, int32_t &DayInWeek)
{
    if (g_bTest)
    {
        return true; // 测试模式，不考虑交易时间
    }

    DayInWeek = GetDayInWeek();
    GetTimeInDay(Hour, Min);
    if ((DayInWeek <= 5))
    {
        // 9：30-11：30   13：00-15：00
        if (((Hour >= 9) && (Hour <= 11)) || ((Hour >= 13) && (Hour <= 14)))
        {
            if (((Hour == 9) && (Min < 30)) || ((Hour == 11) && (Min > 30)))
            {
                return false;
            }
            return true;
        }
    }
    return false;
}

struct DealInfo
{
    DealInfo(uint64_t _TransNo, double _Stocks, uint32_t _Copies, double _Sum, OperMode _eType, const std::string &szTime)
    {
        TransNo = _TransNo;
        Stocks = _Stocks;
        Copies = _Copies;
        Sum = _Sum;
        eType = _eType;
        strncpy(szDealTime, szTime.c_str(), sizeof(szDealTime));
        szDealTime[sizeof(szDealTime) - 1] = '\0';
    }

    uint64_t TransNo;
    double Stocks;
    double Sum;
    uint32_t Copies;
    OperMode eType;
    char szDealTime[32];
};

struct StockInfo
{
    StockInfo(double _Stocks, const std::string &szTime)
    {
        Stocks = _Stocks;
        strncpy(szDealTime, szTime.c_str(), sizeof(szDealTime));
        szDealTime[sizeof(szDealTime) - 1] = '\0';
    }

    double Stocks;
    char szDealTime[32];
};

inline void dumpchar(const std::string &szBuff)
{
    printf("%s: ", szBuff.c_str());
    for (size_t i = 0; i < szBuff.size(); i++)
    {
        printf("[%c]-(%d) ", szBuff[i], szBuff[i]);
    }
    printf("\n");
}

inline double PrettyDouble(double f)
{
    return double(int32_t((f + 0.0005) * 1000)) / 1000;
}

#endif