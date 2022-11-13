#pragma once

#include <stdint.h>
#include <functional>
#include <string.h>
#include <map>
#include <mutex>
#include <vector>

#include "Log.h"


enum class OperMode : uint32_t
{
    Sale, // 卖出
    Buy,  // 买入
};

inline const char *FormatOperMode(OperMode eMode)
{
    switch (eMode)
    {
    case OperMode::Sale:
        return "卖出";
    case OperMode::Buy:
        return "买入";
    default:
        return "未知";
    }
}

// 交易策略基类
class ITradeStrategy
{
public:
public:
    struct Deal 
    {
        Deal(uint64_t _TransationNo, double _Sum, double _Stocks, uint32_t _Copies, const char *_DealTime, OperMode _Oper)
        {
            Sum = _Sum;
            Stocks = _Stocks;
            Copies = _Copies;
            TransationNo = _TransationNo;
            strncpy(DealTime, _DealTime, sizeof(DealTime));
            DealTime[sizeof(DealTime) - 1] = '\0';
            Oper = _Oper;
        }

        void Details() const
        {
            LOG_INFO("成交时间[%s], 事务号[%lu], 操作类型[%s], 成交额[%0.3lf], 股价[%0.3lf], 份数[%u]",
                     DealTime, TransationNo, FormatOperMode(Oper), Sum, Stocks, Copies);
        }

        char                         DealTime[32];           // 成交时间
        double                       Sum;                    // 总交易额
        double                       Stocks;                 // 成交价
        uint64_t                     TransationNo;           // 事务号
        uint32_t                     Copies;                 // 份数
        OperMode                     Oper;                   // 卖出还是买入
    };

    ~ITradeStrategy() = default;

    // 压力位，阻力位，总资产，回调, 交易佣金比例, 网格宽度, 一格所含份数
    virtual void Init(double TopStock,
                      double LowStock,
                      double Asset,
                      const std::function<int32_t(uint64_t identify, double Stocks, uint32_t Copies)> &SaleCallback,
                      const std::function<int32_t(uint64_t identify, double Stocks, uint32_t Copies)> &BuyCallback,
                      double Commission = 0.0025,
                      double GridWidth = 0.05,
                      double WidthCopies = 100)
    {
        TopStock_        = TopStock;
        LowStock_        = LowStock;
        InitAsset_       = Asset;
        SaleCallback_    = SaleCallback;
        BuyCallback_     = BuyCallback;
        CommissionRatio_ = Commission;
        GridWidth_       = GridWidth;
        WidthCopies_     = (WidthCopies < 100) ? 100 : WidthCopies;
        WidthCopies_     = WidthCopies_ / 100 * 100; // 必须是100的整数倍
    }

    // 股价更新
    virtual void Update(double Stocks) = 0;

    // 更新收盘价
    virtual void UpdateClosed(double Stocks) = 0;

    // 确认卖出  单价，份数, 返回事务号
    virtual uint64_t ComfirmSale(double Stocks, uint32_t Copies, double Sum, const char *DealTime) = 0;

    // 确认买入  单价，份数，成交时间, 返回事务号
    virtual uint64_t ComfirmBuy(double Stocks, uint32_t Copies, double Sum, const char *DealTime) = 0;

    // 总资产
    virtual double TotalAssert() = 0;

    // 收益
    virtual double Yield() = 0;

    // 收益率
    virtual double YieldRatio() = 0;

    // 当前股价
    virtual double GetCurStock() = 0;

    // 获取成交总数
    virtual size_t GetDealCount() = 0;

    // 获取一笔历史成交信息
    virtual const Deal &GetDealAt(size_t Index) = 0;

    // 拷贝一笔历史成交信息
    virtual Deal CopyDealAt(size_t Index) = 0;

    virtual std::recursive_mutex &GetMutex() = 0;

    // 获取总份数
    virtual uint32_t GetCopies() = 0;

    // 获取剩余可用资产
    virtual double GetFreeAssert() = 0;

    // 获取股价所在网格的位置
    virtual size_t GetGridPostion(double Stock) = 0;

    // 获取股价所在网格上界线
    virtual double GetGridUpPostion(double Stock) = 0;

    // 获取股价所在网格下界线
    virtual double GetGridDownPostion(double Stock) = 0;

    // 获取最后一次操作时股价所在网格的位置
    virtual size_t GetLastOperGridPostion() = 0;

    // 获取最后一次操作时股价
    virtual double GetLastOperStocks() = 0;

    // 获取基准股价
    virtual double GetStandardStock() = 0;

    // 获取基准股价网格序号
    virtual size_t GetStandardStockPostion() = 0;

    // 获取当前股价网格序号
    virtual size_t GetCurStockPostion() = 0;

    // 获取股价网格信息
    virtual std::vector<double> GetStockGrids() = 0;

    // 计算买入手续费
    double CalBuyServiceFee(double Stocks, uint32_t Copies)
    {
        double sum = Stocks * Copies;
        double TransferFee = sum * TransferFeeRatio_; // 过户费
        double Commission = sum * CommissionRatio_;   // 交易佣金
        return TransferFee + Commission;
    }

    // 计算卖出手续费
    double CalSaleServiceFee(double Stocks, uint32_t Copies)
    {
        double sum = Stocks * Copies;
        double StampTax = sum * StampTaxRatio_;            // 印花税
        double TransferFee = sum * TransferFeeRatio_;      // 过户费
        double Commission = sum * CommissionRatio_;        // 交易佣金
        return StampTax + TransferFee + Commission;
    }

protected:
    double            StampTaxRatio_       = 0.001; // 印花税
    double            TransferFeeRatio_    = 0.002; // 过户费比例

    double            StandardStock_       = 0.0;   // 基准股价
    double            TopStock_            = 0.0;   // 压力位股价
    double            LowStock_            = 0.0;   // 阻力位股价
    double            InitAsset_           = 0.0;   // 初始资产
    double            CommissionRatio_     = 0.003; // 交易佣金比例
    double            GridWidth_           = 0.05;  // 网格宽度
    uint32_t          WidthCopies_         = 100;   // 一格的份数，必须大于等于100，且是100的整数倍

    volatile uint64_t TransactionNo        = 0;     // 事务号
    std::function<int32_t(uint64_t identify, double Stocks, uint32_t Copies)> SaleCallback_;
    std::function<int32_t(uint64_t identify, double Stocks, uint32_t Copies)> BuyCallback_;
};

