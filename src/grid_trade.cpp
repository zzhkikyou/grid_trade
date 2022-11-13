
#include "grid_trade.h"

/*
    突破压力位和阻力位时，应该卖掉股票，及时止盈或者止损
    压力位 ------------------------------------
        4 -------------------------------------
        3 -------------------------------------
        2 -------------------------------------
        1 -------------------------------------
        0 -------------------------------------
    阻力位 -------------------------------------

    压力位和阻力位应该是传入的
*/

// 创建网格 此时应该不考虑资金是否足够
void GridTrade::CreateGrid()
{
    std::set<double> StockGridSet; // 股价网格线集合
    StockGridSet.insert(StandardStock_);
    StockGridSet.insert(TopStock_);
    StockGridSet.insert(LowStock_);

    double UpTmp = StandardStock_ * (1 + GridWidth_);
    while (UpTmp < TopStock_)
    {
        StockGridSet.insert(UpTmp);
        UpTmp *= (1 + GridWidth_);
    }

    double DownTmp = StandardStock_ * (1 - GridWidth_);
    while (DownTmp > LowStock_)
    {
        StockGridSet.insert(DownTmp);
        DownTmp *= (1 - GridWidth_);
    }

    for (auto &it : StockGridSet)
    {
        StockGrids.push_back(it);
        if (it == StandardStock_)
        {
            StandardPostion_ = StockGrids.size() - 1; // 记录标准股价的网格序号
        }
    }

    LOG_INFO("网格建立完成，网格数量 %lu 个, 基准股价所在第 %lu 格", StockGridSet.size(), StandardPostion_);
}

// 获取股价所在网格上界线
double GridTrade::GetGridUpPostion(double Stock)
{
    std::unique_lock<std::recursive_mutex> lock(m_mutex);
    if (StockGrids.empty())
    {
        return 0.0; // 超过网格
    }
    if (Stock < StockGrids.front() || Stock > StockGrids.back())
    {
        return 0.0; // 超过网格
    }
    for (size_t i = 0; i < StockGrids.size(); i++)
    {
        if (StockGrids[i] > Stock)
        {
            return StockGrids[i];
        }
    }
    return 0.0; // 超过网格
}

// 获取股价所在网格下界线
double GridTrade::GetGridDownPostion(double Stock)
{
    std::unique_lock<std::recursive_mutex> lock(m_mutex);
    if (StockGrids.empty())
    {
        return 0.0; // 超过网格
    }
    if (Stock < StockGrids.front() || Stock > StockGrids.back())
    {
        return 0.0; // 超过网格
    }
    for (size_t i = 0; i < StockGrids.size(); i++)
    {
        if (StockGrids[i] > Stock)
        {
            return StockGrids[i - 1]; 
        }
    }
    return 0.0; // 超过网格
}

// 获取股价所在网格的位置
size_t GridTrade::GetGridPostion(double Stock)
{
    std::unique_lock<std::recursive_mutex> lock(m_mutex);
    if (StockGrids.empty())
    {
        return (size_t)-1; // 超过网格
    }

    if (Stock < StockGrids.front() || Stock > StockGrids.back())
    {
        return (size_t)-1; // 超过网格
    }

    for (size_t i = 0; i < StockGrids.size(); i++)
    {
        if (StockGrids[i] == Stock)
        {
            return i;
        }
        if (StockGrids[i] > Stock)
        {
            return i - 1; // 返回前一个网格的下标
        }
    }
    return (size_t)-1; // 超过网格
}

// 基准股价, 压力位，阻力位，总资产，回调, 交易佣金, 网格宽度
void GridTrade::Init(double TopStock,
                     double LowStock,
                     double Asset,
                     const std::function<int32_t(uint64_t identify, double Stocks, uint32_t Copies)> &SaleCallback,
                     const std::function<int32_t(uint64_t identify, double Stocks, uint32_t Copies)> &BuyCallback,
                     double Commission,
                     double GridWidth,
                     double WidthCopies)
{
    ITradeStrategy::Init(TopStock, LowStock, Asset, SaleCallback, BuyCallback, Commission, GridWidth, WidthCopies);
    FreeAssert_ = InitAsset_;
}

// 计算可以买入的最大份额
uint32_t GridTrade::CalculateBuyCopies(double Assert ,double Stocks, uint32_t Copies)
{
    if (Copies < 100)
    {
        return 0;
    }
    Copies = Copies / 100 * 100;
    while (Copies)
    {
        if (Assert >= (CalBuyServiceFee(Stocks, Copies) + Stocks * Copies))
        {
            break;
        }
        Copies -= 100; 
    }
    return Copies;
}


// 股价更新
void GridTrade::Update(double Stocks)
{
    std::unique_lock<std::recursive_mutex> lock(m_mutex);
    LOG_DEBUG("====>> 股价更新 : 从 %.3lf 元 到 %.3lf 元", CurStock_, Stocks);
    CurStock_ = Stocks;
    if (StockGrids.empty()) // 网格还不存在，建半仓
    {
        LOG_WARN("网格还不存在, 请先建半仓");
        return;
    }

    auto CurPostion = GetGridPostion(Stocks);
    LOG_DEBUG("当前股价在第 %lu 格", CurPostion);
    if (CurPostion == (size_t)-1)
    {
        LOG_ERROR("当前股价 %0.3lf 超过网格, 建议全部卖出，重新开始", Stocks);
        return;
    }

    if (CurPostion < LastOperPostion_) // 跌了，并且超过至少一个网格，要买入
    {
        uint32_t Copies = CalculateBuyCopies(FreeAssert_, Stocks, (uint32_t)(LastOperPostion_ - CurPostion) * WidthCopies_); 
        if (Copies)
        {
            LOG_DEBUG("建议买入股价 %.3lf 元, 份数 %u, 大概总开销 %.3lf 元, 当前剩余可用资产 %.3lf 元, 总份数 %u",
                      Stocks, Copies, CalBuyServiceFee(Stocks, Copies) + Stocks * Copies, FreeAssert_, TotalCopies_);
            BuyCallback_(identify, Stocks, Copies);
            identify++;
        }
        else
        {
            LOG_WARN("没有足够资产购买");
        }
    }
    else if(CurPostion > LastOperPostion_) // 涨了，并且超过至少一个网格，要卖出
    {
        uint32_t Copies = std::min((uint32_t)(CurPostion - LastOperPostion_) * WidthCopies_, TotalCopies_);
        if (Copies)
        {
            LOG_DEBUG("建议卖出股价 %.3lf 元, 份数 %u, 大概总成交 %.3lf 元, 当前剩余可用资产 %.3lf 元, 总份数 %u",
                      Stocks, Copies, Stocks * Copies - CalSaleServiceFee(Stocks, Copies), FreeAssert_, TotalCopies_);
            SaleCallback_(identify, Stocks, Copies);
            identify++;
        }
        else
        {
            LOG_WARN("没有足够份数卖出");
        }
    }
}

// 更新收盘价
void GridTrade::UpdateClosed(double Stocks)
{
    CurStock_ = Stocks;
}

// 确认卖出  单价，份数
uint64_t GridTrade::ComfirmSale(double Stocks, uint32_t Copies, double Sum, const char *DealTime)
{
    std::unique_lock<std::recursive_mutex> lock(m_mutex);

    if (TotalCopies_ < Copies)
    {
        LOG_ERROR("确认卖出失败, 总份数 %u 不可以比卖出份数 %u 小", TotalCopies_, Copies);
        return - 1;
    }

    auto CurTransactionNo = TransactionNo++;

    TotalCopies_ -= Copies;
    FreeAssert_ += Sum;
    LastOperPostion_ = GetGridPostion(Stocks);
    LastOperStock_ = Stocks;

    Deals.insert(Deals.end(), {CurTransactionNo, Sum, Stocks, Copies, DealTime, OperMode::Sale});
    LOG_WARN("事务[%lu], 确认卖出股价 %.3lf 元, 份数 %u, 成交额 %.3lf 元, 当前剩余可用资产 %.3lf 元, 总份数 %u, 总资产 %.3lf 元, 收益 %.3lf 元, 收益率 %.3lf%%",
             CurTransactionNo, Stocks, Copies, Sum, FreeAssert_, TotalCopies_, TotalAssert(), Yield(), YieldRatio() * 100);

    return CurTransactionNo;
}

// 确认买入  单价，份数，成交额, 成交时间
uint64_t GridTrade::ComfirmBuy(double Stocks, uint32_t Copies, double Sum, const char *DealTime)
{
    std::unique_lock<std::recursive_mutex> lock(m_mutex);

    if (StockGrids.empty()) // 第一次买入，开始建立网格
    {
        TotalCopies_ = Copies;
        StandardStock_ = Stocks;
        FreeAssert_ -= Sum;
        CreateGrid();
        LastOperPostion_ = StandardPostion_;
        LastOperStock_ = Stocks;
    }
    else
    {
        TotalCopies_ += Copies;
        FreeAssert_ -= Sum;
        LastOperPostion_ = GetGridPostion(Stocks);
        LastOperStock_ = Stocks;
    }

    auto CurTransactionNo = TransactionNo++;

    Deals.insert(Deals.end(), {CurTransactionNo, Sum, Stocks, Copies, DealTime, OperMode::Buy});

    LOG_WARN("事务[%lu], 确认买入股价 %.3lf 元, 份数 %u, 成交额 %.3lf 元, 当前剩余可用资产 %.3lf 元, 总份数 %u, 总资产 %.3lf 元, 收益 %.3lf 元, 收益率 %.3lf%%",
             CurTransactionNo, Stocks, Copies, Sum, FreeAssert_, TotalCopies_, TotalAssert(), Yield(), YieldRatio() * 100);

    return CurTransactionNo;
}

// 总资产
double GridTrade::TotalAssert()
{
    return FreeAssert_ + TotalCopies_ * CurStock_;
}

// 收益
double GridTrade::Yield()
{
    return TotalAssert() - InitAsset_;
}

// 收益率
double GridTrade::YieldRatio()
{
    return Yield() / InitAsset_;
}

// 当前股价
double GridTrade::GetCurStock()
{
    return CurStock_;
}

// 获取成交总数
size_t GridTrade::GetDealCount()
{
    std::unique_lock<std::recursive_mutex> lock(m_mutex);
    return Deals.size();
}

// 获取一笔历史成交信息
const ITradeStrategy::Deal &GridTrade::GetDealAt(size_t Index)
{
    std::unique_lock<std::recursive_mutex> lock(m_mutex);
    return Deals.at(Index);
}

// 拷贝一笔历史成交信息
ITradeStrategy::Deal GridTrade::CopyDealAt(size_t Index)
{
    std::unique_lock<std::recursive_mutex> lock(m_mutex);
    return Deals.at(Index);
}

// 获取总份数
uint32_t GridTrade::GetCopies()
{
    return TotalCopies_;
}

// 获取剩余可用资产
double GridTrade::GetFreeAssert()
{
    return FreeAssert_;
}

// 获取当前股价网格序号
size_t GridTrade::GetCurStockPostion()
{
    if (StockGrids.empty()) // 网格还不存在
    {
        return (uint64_t)-1;
    }
    return GetGridPostion(CurStock_);
}