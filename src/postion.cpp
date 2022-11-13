#include "postion.h"
#include "Log.h"

void PostionMgr::ConfirmBuy(uint64_t _TransationNo, double _Stocks, uint32_t _Copies, const std::string &DealTime)
{
    std::unique_lock<std::mutex> lock(Mutex);
    Postions.push({_TransationNo, _Stocks, _Copies, DealTime});
}

void PostionMgr::ConfirmSale(uint64_t _TransationNo, double _Stocks, uint32_t _Copies, const std::string &DealTime)
{
    std::unique_lock<std::mutex> lock(Mutex);
    auto &ClosingPostion = ClosingPostions[_TransationNo];

    if (Postions.empty())
    {
        LOG_ERROR("操作不合理! 当前已经没有仓位, 无法确认卖出份数 %u", _Copies);
        ClosingPostion.insert(ClosingPostion.end(), FORMAT("操作不合理! 当前已经没有仓位, 无法确认卖出份数 %u", _Copies));
        return;
    }

    if (_Stocks < Postions.top().Stocks)
    {
        LOG_ERROR("操作存在亏损! 卖出的股价 %.3lf 元小于未平仓的事务[%lu] 股价 %.3lf 元", _Stocks, Postions.top().TransationNo, Postions.top().Stocks);
        ClosingPostion.insert(ClosingPostion.end(), FORMAT("操作存在亏损! 卖出的股价 %.3lf 元小于未平仓的事务[%lu] 股价 %.3lf 元", _Stocks, Postions.top().TransationNo, Postions.top().Stocks));
    }
    else
    {
        while (_Copies && !Postions.empty())
        {
            if (_Copies >= Postions.top().Copies)
            {
                _Copies -= Postions.top().Copies;
                LOG_WARN("完全平仓事务[%lu], 股价 %.3lf 元, 份数[%u], 剩余[%u]", Postions.top().TransationNo, Postions.top().Stocks, _Copies, Postions.top().Copies);
                ClosingPostion.insert(ClosingPostion.end(), FORMAT("完全平仓事务[%lu], 股价 %.3lf 元, 份数[%u], 剩余[%u]", Postions.top().TransationNo, Postions.top().Stocks, _Copies, Postions.top().Copies));
                Postions.pop();
            }
            else
            {
                Postions.top().Copies -= _Copies;
                LOG_WARN("部分平仓事务[%lu], 股价 %.3lf 元, 份数[%u], 剩余[%u]", Postions.top().TransationNo, Postions.top().Stocks,  _Copies, Postions.top().Copies);
                ClosingPostion.insert(ClosingPostion.end(), FORMAT("部分平仓事务[%lu], 股价 %.3lf 元, 份数[%u], 剩余[%u]", Postions.top().TransationNo, Postions.top().Stocks, _Copies, Postions.top().Copies));
                _Copies = 0;
            }
        }
        if (_Copies)
        {
            LOG_ERROR("操作不合理! 当前已经没有仓位, 无法确认卖出剩余份数 %u", _Copies);
            ClosingPostion.insert(ClosingPostion.end(), FORMAT("操作不合理! 当前已经没有仓位, 无法确认卖出剩余份数 %u", _Copies));
            return;
        }
    }
}