#include "Log.h"
#include "strategy.h"
#include "grid_trade.h"
#include "format_time.h"
#include "postion.h"
#include "persistence.h"
#include <memory>

// TODO 确定算法的准确性
// TODO 涨价需要卖出时，要找到历史上匹配的那笔买入，一定要比当时贵或者相等 ！！！
// TODO api json 重演
// TODO http parse

struct TradeInfo
{
    TradeInfo(uint64_t _TransNo, double _Stocks, uint32_t _Copies, OperMode _eType, const std::string &szTime)
    {
        TransNo = _TransNo;
        Stocks = _Stocks;
        Copies = _Copies;
        eType = _eType;
        strncpy(szDealTime, szTime.c_str(), sizeof(szDealTime));
        szDealTime[sizeof(szDealTime) - 1] = '\0';
    }

    uint64_t TransNo;
    double Stocks;
    uint32_t Copies;
    OperMode eType;
    char szDealTime[32];
};

void Trade(double TopStock, double LowStock, double Asset, double GridWidth, double WidthCopies)
{
    std::unique_ptr<GridTrade> upStrategy{new GridTrade};
    std::unique_ptr<PostionMgr> upPostionMgr{new PostionMgr};
    std::unique_ptr<PersistenceWriter> upWriter{new PersistenceWriter};

    remove("Trade.dat");
    upWriter->Open("Trade.dat");

    auto OnSale = [&](double Stocks, uint32_t Copies) -> int32_t
    {
        auto szTime = GetLocalTime();
        auto TransNo = upStrategy->ComfirmSale(Stocks, Copies, upStrategy->CalSaleServiceFee(Stocks, Copies) + Stocks * Copies, szTime.c_str());
        upPostionMgr->ConfirmSale(TransNo, Stocks, Copies);

        TradeInfo Info(TransNo, Stocks, Copies, OperMode::Sale, szTime);
        upWriter->Write(&Info, sizeof(TradeInfo));
        return 0;
    };

    auto OnBuy = [&](double Stocks, uint32_t Copies) -> int32_t
    {
        auto szTime = GetLocalTime();
        auto TransNo = upStrategy->ComfirmBuy(Stocks, Copies, upStrategy->CalBuyServiceFee(Stocks, Copies) + Stocks * Copies, szTime.c_str());
        upPostionMgr->ConfirmBuy(TransNo, Stocks, Copies);

        TradeInfo Info(TransNo, Stocks, Copies, OperMode::Buy, szTime);
        upWriter->Write(&Info, sizeof(TradeInfo));
        return 0;
    };
    upStrategy->Init(TopStock, LowStock, Asset, OnSale, OnBuy, 0.0025, GridWidth, WidthCopies);

    upStrategy->Update(30);

    upStrategy->Update(20);

    upStrategy->Update(30);

    upStrategy->Update(40);

    upStrategy->Update(30);

    auto DealCount = upStrategy->GetDealCount();

    LOG_WARN("展示历史成交信息");
    for (size_t i = 0; i < DealCount; i++)
    {
        upStrategy->GetDealAt(i).Details();
    }

    LOG_WARN("最终成果展示: 初始资产 %0.3lf, 最终资产 %0.3lf, 收益 %0.3lf, 收益率 %0.3lf%%", Asset, upStrategy->TotalAssert(), upStrategy->Yield(), upStrategy->YieldRatio() * 100);
}

void Replay(double TopStock, double LowStock, double Asset, double GridWidth, double WidthCopies)
{
    std::unique_ptr<GridTrade> upStrategy{new GridTrade};
    std::unique_ptr<PostionMgr> upPostionMgr{new PostionMgr};
    std::unique_ptr<PersistenceWriter> upWriter{new PersistenceWriter};
    std::unique_ptr<PersistenceReader> upReader{new PersistenceReader};
    upWriter->Open("Trade.dat");
    upReader->Open("Trade.dat");

    auto OnSale = [&](double Stocks, uint32_t Copies) -> int32_t
    {
        auto szTime = GetLocalTime();
        auto TransNo = upStrategy->ComfirmSale(Stocks, Copies, upStrategy->CalSaleServiceFee(Stocks, Copies) + Stocks * Copies, szTime.c_str());
        upPostionMgr->ConfirmSale(TransNo, Stocks, Copies);

        TradeInfo Info(TransNo, Stocks, Copies, OperMode::Sale, szTime);
        upWriter->Write(&Info, sizeof(TradeInfo));
        return 0;
    };

    auto OnBuy = [&](double Stocks, uint32_t Copies) -> int32_t
    {
        auto szTime = GetLocalTime();
        auto TransNo = upStrategy->ComfirmBuy(Stocks, Copies, upStrategy->CalBuyServiceFee(Stocks, Copies) + Stocks * Copies, szTime.c_str());
        upPostionMgr->ConfirmBuy(TransNo, Stocks, Copies);

        TradeInfo Info(TransNo, Stocks, Copies, OperMode::Buy, szTime);
        upWriter->Write(&Info, sizeof(TradeInfo));
        return 0;
    };

    upStrategy->Init(TopStock, LowStock, Asset, OnSale, OnBuy, 0.0025, GridWidth, WidthCopies);

    TradeInfo *lpInfo;
    uint32_t Size;

    while ((lpInfo = (TradeInfo *)upReader->Read(Size)) != nullptr)
    {
        if (lpInfo->eType == OperMode::Buy)
        {
            auto TransNo = upStrategy->ComfirmBuy(lpInfo->Stocks, lpInfo->Copies, upStrategy->CalBuyServiceFee(lpInfo->Stocks, lpInfo->Copies) + lpInfo->Stocks * lpInfo->Copies, lpInfo->szDealTime);
            upPostionMgr->ConfirmBuy(TransNo, lpInfo->Stocks, lpInfo->Copies);
        }
        else
        {
            auto TransNo = upStrategy->ComfirmSale(lpInfo->Stocks, lpInfo->Copies, upStrategy->CalSaleServiceFee(lpInfo->Stocks, lpInfo->Copies) + lpInfo->Stocks * lpInfo->Copies, lpInfo->szDealTime);
            upPostionMgr->ConfirmSale(TransNo, lpInfo->Stocks, lpInfo->Copies);
        }
    }
    auto DealCount = upStrategy->GetDealCount();

    LOG_WARN("展示历史成交信息");
    for (size_t i = 0; i < DealCount; i++)
    {
        upStrategy->GetDealAt(i).Details();
    }

    upStrategy->Update(30);
    LOG_WARN("最终成果展示: 初始资产 %0.3lf, 最终资产 %0.3lf, 收益 %0.3lf, 收益率 %0.3lf%%", Asset, upStrategy->TotalAssert(), upStrategy->Yield(), upStrategy->YieldRatio() * 100);
}

int main()
{
    Log::GetInstance()->SetLogLevel(Log::LEVEL::TRACE);

    LOG_ALARM("启动网格交易");
    Trade(60, 15, 60000, 0.05, 100);
    LOG_ALARM("退出网格交易");

    // LOG_ALARM("开始重演");
    // Replay(60, 15, 60000, 0.05, 100);
    // LOG_ALARM("退出重演");
}