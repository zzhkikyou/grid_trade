#ifndef _POSTION_H_
#define _POSTION_H_

#include <stdint.h>
#include <stack>
#include <map>
#include <string>
#include <vector>
#include <mutex>

// 仓位管理
class PostionMgr
{
public:
    // 仓位信息
    struct Postion
    {
        Postion(uint64_t _TransationNo, double _Stocks, uint32_t _Copies, const std::string &_DealTime)
            : TransationNo(_TransationNo), Stocks(_Stocks), Copies(_Copies), DealTime(_DealTime)
        {
        }
        uint64_t                     TransationNo;               // 事务号
        double                       Stocks;                     // 股价
        uint32_t                     Copies;                     // 剩余份数
        std::string                  DealTime;
    };

    PostionMgr() = default;

    void ConfirmBuy(uint64_t _TransationNo, double _Stocks, uint32_t _Copies, const std::string &DealTime);
    void ConfirmSale(uint64_t _TransationNo, double _Stocks, uint32_t _Copies, const std::string &DealTime);

    std::stack<Postion> CopyPostion()
    {
        std::unique_lock<std::mutex> lock(Mutex);
        return Postions;
    }

    std::map<uint64_t, std::vector<std::string>> CopyClosingPostions()
    {
        std::unique_lock<std::mutex> lock(Mutex);
        return ClosingPostions;
    }

private:
    std::mutex                                   Mutex;
    std::stack<Postion>                          Postions;                    // 仓位
    std::map<uint64_t, std::vector<std::string>> ClosingPostions;             // 平仓情况
};

#endif