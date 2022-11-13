#ifndef _GRID_TRADE_H_
#define _GRID_TRADE_H_

#include "strategy.h"
#include "Log.h"

#include <string>
#include <vector>
#include <set>

class GridTrade : public ITradeStrategy
{
public:

    // 压力位，阻力位，总资产，回调, 交易佣金比例, 网格宽度, 一格所含份数
    virtual void Init(double TopStock,
                      double LowStock,
                      double Asset,
                      const std::function<int32_t(uint64_t identify, double Stocks, uint32_t Copies)> &SaleCallback,
                      const std::function<int32_t(uint64_t identify, double Stocks, uint32_t Copies)> &BuyCallback,
                      double Commission,
                      double GridWidth,
                      double WidthCopies);

    // 计算可以买入的最大份额
    uint32_t CalculateBuyCopies(double Assert, double Stocks, uint32_t Copies);

    // 股价更新
    virtual void Update(double Stocks);

    // 更新收盘价
    virtual void UpdateClosed(double Stocks);

    // 确认卖出  单价，份数
    virtual uint64_t ComfirmSale(double Stocks, uint32_t Copies, double Sum, const char *DealTime);

    // 确认买入  单价，份数，成交时间
    virtual uint64_t ComfirmBuy(double Stocks, uint32_t Copies, double Sum, const char *DealTime);

    // 总资产
    virtual double TotalAssert();

    // 收益
    virtual double Yield();

    // 收益率
    virtual double YieldRatio();

    // 当前股价
    virtual double GetCurStock();

    // 获取成交总数
    virtual size_t GetDealCount();

    // 获取一笔历史成交信息
    virtual const Deal &GetDealAt(size_t Index);

    // 拷贝一笔历史成交信息
    virtual Deal CopyDealAt(size_t Index);

    virtual std::recursive_mutex &GetMutex() { return m_mutex; };

    // 获取总份数
    virtual uint32_t GetCopies();

    // 获取剩余可用资产
    virtual double GetFreeAssert();

    // 获取股价所在网格的位置
    virtual size_t GetGridPostion(double Stock);

    // 获取股价所在网格上界线
    virtual double GetGridUpPostion(double Stock);

    // 获取股价所在网格下界线
    virtual double GetGridDownPostion(double Stock);

    // 获取最后一次操作时股价所在网格的位置
    virtual size_t GetLastOperGridPostion() { return LastOperPostion_; }

    // 获取最后一次操作时股价
    virtual double GetLastOperStocks() { return LastOperStock_; }

    // 获取基准股价
    virtual double GetStandardStock() { return StandardStock_; }

    // 获取基准股价网格序号
    virtual size_t GetStandardStockPostion() { return StandardPostion_; }

    // 获取当前股价网格序号
    virtual size_t GetCurStockPostion();

    // 获取股价网格信息
    virtual std::vector<double> GetStockGrids() { return StockGrids; }

private:
    // 创建网格 此时应该不考虑资金是否足够
    void CreateGrid();

private:
    double                LowStock            = 0.0;          // 阻力位
    double                highStock           = 0.0;          // 压力位
    std::vector<double>   StockGrids;                         // 股价网格线集合 (有序)
    size_t                StandardPostion_;                   // 基准股价的网格序号

    double                FreeAssert_         = 0.0;          // 空闲资产

    double                CurStock_           = 0.0;          // 当前股价
    double                LastOperStock_      = 0.0;          // 最后一次操作的股价
    size_t                LastOperPostion_    = 0;            // 最后一次操作的网格序号

    uint32_t              TotalCopies_        = 0;             // 总份额
    std::vector<Deal>     Deals;                               // 历史成交 按时间先后顺序

    std::recursive_mutex  m_mutex;
    volatile uint64_t     identify            = 0;             // 回调标识  同一个网格的交易内容，使用相同的标识
};

#endif