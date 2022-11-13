#include "common.h"

thread_local std::string Log::threadname_;
thread_local uint32_t Log::tid_ = 0;

std::string szConfigPath;
std::string szPrefix;
std::string StockCode;
double TopStock;
double LowStock;
double InitAssert;
double GridWidth;
uint32_t WidthCopies;

volatile bool g_bTest = false;
volatile bool bReplayTime = false;

notify Notify;

int32_t OnSale(uint64_t identify, double Stocks, uint32_t Copies)
{
    if (bReplayTime)
    {
        return 0;
    }
    std::string szTitle("建议卖出");
    std::string szMsg;

    szMsg += "  建议卖出: 高于等于股价 [";
    szMsg += std::to_string(Stocks);
    szMsg += "] 元, 份数 [";
    szMsg += std::to_string(Copies);
    szMsg += "] ";
    szMsg += "总成交价大约 [";
    szMsg += std::to_string(PrettyDouble(Stocks * Copies - GetGridTrade()->CalSaleServiceFee(Stocks, Copies)));
    szMsg += "] 元\n";

    auto TmpPostions = GePostionMgr()->CopyPostion();
    szMsg += "  当前所有仓位: \n";

    auto CurTime = ResolveLocalTime(GetLocalTime());

    while (!TmpPostions.empty())
    {
        szMsg += FORMAT("       事务[%lu] 号, 股价[%.3lf] 元, 份数[%u]", TmpPostions.top().TransationNo, TmpPostions.top().Stocks, TmpPostions.top().Copies);
        TmpPostions.pop();
    }

    szMsg += "  若成功卖出: \n";

    bool HasSaledPostion = false;
    auto _Copies = Copies;
    auto Postions = GePostionMgr()->CopyPostion();
    while (_Copies && !Postions.empty())
    {
        auto DealTime = ResolveLocalTime(Postions.top().DealTime);
        if ((DealTime.Year == CurTime.Year) && (DealTime.month == CurTime.month) && (DealTime.Day == CurTime.Day)) // 当天买的不能当天卖
        {
            Postions.pop();
            continue;
        }

        if (_Copies >= Postions.top().Copies)
        {
            _Copies -= Postions.top().Copies;
            szMsg += FORMAT("       将完全平仓事务[%lu] 号, 股价[%.3lf] 元, 份数[%u]", Postions.top().TransationNo, Postions.top().Stocks, Postions.top().Copies);
            Postions.pop();
            HasSaledPostion = true;
        }
        else
        {
            Postions.top().Copies -= _Copies;
            szMsg += FORMAT("       将部分平仓事务[%lu] 号, 股价[%.3lf] 元, 份数[%u], 剩余[%u]", Postions.top().TransationNo, Postions.top().Stocks, _Copies, Postions.top().Copies);
            _Copies = 0;
            HasSaledPostion = true;
        }
    }

    if (HasSaledPostion)
    {
        Notify.notify_zzh(identify, szTitle, szMsg);
    }

    return 0;
}

int32_t OnBuy(uint64_t identify, double Stocks, uint32_t Copies)
{
    if (bReplayTime)
    {
        return 0;
    }
    std::string szTitle("建议买入");
    std::string szMsg;

    szMsg += "  建议买入小于等于: 股价 [";
    szMsg += std::to_string(Stocks);
    szMsg += "] 元, 份数 [";
    szMsg += std::to_string(Copies);
    szMsg += "], 总成交价大约 [";
    szMsg += std::to_string(PrettyDouble(GetGridTrade()->CalBuyServiceFee(Stocks, Copies) + Stocks * Copies));
    szMsg += "] 元\n";

    auto TmpPostions = GePostionMgr()->CopyPostion();
    szMsg += "  当前所有仓位: \n";

    while (!TmpPostions.empty())
    {
        szMsg += FORMAT("       事务[%lu] 号, 股价[%.3lf] 元, 份数[%u]", TmpPostions.top().TransationNo, TmpPostions.top().Stocks, TmpPostions.top().Copies);
        TmpPostions.pop();
    }

    Notify.notify_zzh(identify, szTitle, szMsg);
    return 0;
}

void InitStrategy(double TopStock, double LowStock, double Asset, double GridWidth, double WidthCopies)
{
    GetGridTrade()->Init(TopStock, LowStock, Asset, OnSale, OnBuy, 0.0025, GridWidth, WidthCopies);
    LOG_INFO("初始化策略: 最高价: %.3lf 元, 最低价: %.3lf 元, 初始资产: %.3lf 元, 网格宽度: %.3lf, 份数: %lf", TopStock, LowStock, Asset, GridWidth, WidthCopies);

    std::string DataPath = szPrefix + "_data";
    ShellExec("mkdir -p " + DataPath);
    auto TimeInfo = ResolveLocalTime(GetLocalTime());
    std::string szStockInfo;
    szStockInfo = DataPath + "/stocks_" + std::to_string(TimeInfo.Year) + "_" + std::to_string(TimeInfo.month) + "_" + std::to_string(TimeInfo.Day) + ".dat";
    GePersistenceStockWriter()->Open(szStockInfo);
    GePersistenceStockReader()->Open(szStockInfo);
}

void Replay()
{
    bReplayTime = true;
    std::string DataPath = szPrefix + "_data";
    ShellExec("mkdir -p " + DataPath);
    GePersistenceWriter()->Open(DataPath + "/Deals.dat");
    GePersistenceReader()->Open(DataPath + "/Deals.dat");
    DealInfo *lpInfo = nullptr;
    uint32_t Size = 0;
    while ((!IsTerminated()) && ((lpInfo = (DealInfo *)GePersistenceReader()->Read(Size)) != nullptr))
    {
        if (lpInfo->eType == OperMode::Buy)
        {
            auto TransNo = GetGridTrade()->ComfirmBuy(lpInfo->Stocks, lpInfo->Copies, lpInfo->Sum, lpInfo->szDealTime);
            GePostionMgr()->ConfirmBuy(TransNo, lpInfo->Stocks, lpInfo->Copies, lpInfo->szDealTime);
        }
        else
        {
            auto TransNo = GetGridTrade()->ComfirmSale(lpInfo->Stocks, lpInfo->Copies, lpInfo->Sum, lpInfo->szDealTime);
            GePostionMgr()->ConfirmSale(TransNo, lpInfo->Stocks, lpInfo->Copies, lpInfo->szDealTime);
        }
    }
    bReplayTime = false;
    LOG_INFO("重演结束");
}


HttpAssembler httpAssem;
void InitReqStocks()
{
    httpAssem.AddOption("Accept-Language", "zh-cn;q=0.8,en-US;q=0.9");
    httpAssem.AddOption("Accept-Charset", "utf-8, iso-8859-1;q=0.5");
    httpAssem.AddOption("Connection", "keep-alive");
    httpAssem.AddOption("Host", "qt.gtimg.cn");
    httpAssem.AssembleReq("/q=s_sz" + StockCode);
}

volatile bool g_bTerminated = false;
void sig_handler(int sig)
{
    switch (sig)
    {
    case SIGSEGV:
        LOG_FATAL("捕获到 SIGSEGV 信号, 程序出现重大异常");
        abort();
    case SIGINT:
        LOG_WARN("捕获到 SIGINT 信号, 准备正常退出");
        g_bTerminated = true;
        break;
    default:
        LOG_WARN("捕获到 %d 信号, 暂时不处理", sig);
        break;
    }
}

HttpServer server;
HttpClient client;

void ReqStock()
{
    int32_t Hour, Min, DayInWeek;
    bool bDealTime = !IsDealTime(Hour, Min, DayInWeek);
    while (!g_bTerminated)
    {
        auto Res = IsDealTime(Hour, Min, DayInWeek);
        if (bDealTime != Res)
        {
            bDealTime = Res;
            if (bDealTime)
            {
                LOG_WARN("现在是交易开始时间 %s %d:%d, 准备开始交易", FormatDayInWeek(DayInWeek), Hour, Min);
                std::string szTitle("交易开始");
                std::string szMsg;
                szMsg += "  ";
                szMsg += "现在到了交易开始时间, 做好交易准备";
                Notify.notify_zzh((uint64_t)-1, szTitle, szMsg);
            }
            else
            {
                LOG_WARN("现在是交易截止时间 %s %d:%d, 放松一下", FormatDayInWeek(DayInWeek), Hour, Min);
                std::string szTitle("交易截止");
                std::string szMsg;
                szMsg += "  ";
                szMsg += "现在到了交易截止时间, 放松一下";
                Notify.notify_zzh((uint64_t)-1, szTitle, szMsg);
            }
        }

        if (bDealTime)
        {
            client.RequestStock(httpAssem.GetData());
            std::this_thread::sleep_for(std::chrono::seconds(1)); // 交易时间，则1秒更新一次
        }
        else
        {
            client.RequestStock(httpAssem.GetData());
            std::this_thread::sleep_for(std::chrono::seconds(300)); // 非交易时间，则5分钟更新一次
        }
    }
}

void ReadConfig()
{
    std::string szConfigBuff;
    std::ifstream infile;
    infile.open(szConfigPath, std::ios::out | std::ios::in | std::ios::binary);
    if (infile.is_open())
    {
        std::string line;
        while (getline(infile, line))
        {
            szConfigBuff += line;
        }
    }
    else
    {
        LOG_ERROR("读取配置文件错误");
        exit(0);
    }

    LOG_DEBUG("\n%s", szConfigBuff.c_str());
    cJSON *jRoot = cJSON_Parse(szConfigBuff.c_str());

    cJSON *jItem = cJSON_GetObjectItem(jRoot, "StockCode");
    StockCode = (jItem->valuestring == nullptr ? "" : jItem->valuestring);
    if (StockCode.empty())
    {
        LOG_ERROR("读取配置文件错误, 股票代码为空");
        exit(0);
    }
    szPrefix = StockCode;

    jItem = cJSON_GetObjectItem(jRoot, "TopStock");
    TopStock = jItem->valuedouble;
    if (TopStock == 0.0)
    {
        LOG_ERROR("读取配置文件错误, 最高价为空");
        exit(0);
    }
    jItem = cJSON_GetObjectItem(jRoot, "LowStock");
    LowStock = jItem->valuedouble;
    if (LowStock == 0.0)
    {
        LOG_ERROR("读取配置文件错误, 最低价为空");
        exit(0);
    }

    jItem = cJSON_GetObjectItem(jRoot, "InitAssert");
    InitAssert = jItem->valuedouble;
    if (InitAssert == 0.0)
    {
        LOG_ERROR("读取配置文件错误, 初始资产为空");
        exit(0);
    }

    jItem = cJSON_GetObjectItem(jRoot, "GridWidth");
    GridWidth = jItem->valuedouble;
    if (GridWidth == 0.0)
    {
        LOG_ERROR("读取配置文件错误, 网格宽度为空");
        exit(0);
    }
    jItem = cJSON_GetObjectItem(jRoot, "WidthCopies");
    WidthCopies = (uint32_t)jItem->valueint;
    if (WidthCopies == 0)
    {
        LOG_ERROR("读取配置文件错误, 份数为空");
        exit(0);
    }

    LOG_WARN("配置解析: 最高价[%.3lf], 最低价[%.3lf], 初始资产[%.3lf], 网格宽度[%.3lf], 份数[%u], ", TopStock, LowStock, InitAssert, GridWidth, WidthCopies);
}

// 管理功能
std::string GetPostionStat()
{
    cJSON *jRoot = cJSON_CreateObject();
    cJSON *jArray = cJSON_CreateArray();
    auto Postions = GePostionMgr()->CopyPostion();
    while (!Postions.empty())
    {
        cJSON *Obj = cJSON_CreateObject();
        cJSON *jTransationNo = cJSON_CreateNumber(Postions.top().TransationNo);
        cJSON_AddItemToObject(Obj, "事务号", jTransationNo);
        cJSON *jStocks = cJSON_CreateNumber(PrettyDouble(Postions.top().Stocks));
        cJSON_AddItemToObject(Obj, "成交价", jStocks);
        cJSON *jCopies = cJSON_CreateNumber(Postions.top().Copies);
        cJSON_AddItemToObject(Obj, "份数", jCopies);
        cJSON *jDealTime = cJSON_CreateString(Postions.top().DealTime.c_str());
        cJSON_AddItemToObject(Obj, "成交时间", jDealTime);
        cJSON_AddItemToArray(jArray, Obj);
        Postions.pop();
    }
    cJSON_AddItemToObject(jRoot, "仓位", jArray);
    std::string Res = cJSON_Print(jRoot);
    cJSON_Delete(jRoot);
    return Res;
}

//TODO 最高价 最低价
std::string GetAssertStat()
{
    cJSON *jRoot = cJSON_CreateObject();
    cJSON *jCurStock = cJSON_CreateNumber(PrettyDouble(GetGridTrade()->GetCurStock()));
    cJSON_AddItemToObject(jRoot, "当前股价", jCurStock);
    cJSON *jCurStockPostion = cJSON_CreateNumber(GetGridTrade()->GetCurStockPostion());
    cJSON_AddItemToObject(jRoot, "当前股价网格序号", jCurStockPostion);
    cJSON *jCurStockUpPostion = cJSON_CreateNumber(PrettyDouble(GetGridTrade()->GetGridUpPostion(GetGridTrade()->GetCurStock())));
    cJSON_AddItemToObject(jRoot, "当前股价网格上界线", jCurStockUpPostion);
    cJSON *jCurStockDownPostion = cJSON_CreateNumber(PrettyDouble(GetGridTrade()->GetGridDownPostion(GetGridTrade()->GetCurStock())));
    cJSON_AddItemToObject(jRoot, "当前股价网格下界线", jCurStockDownPostion);
    cJSON *jTotalAssert = cJSON_CreateNumber(PrettyDouble(GetGridTrade()->TotalAssert()));
    cJSON_AddItemToObject(jRoot, "总资产", jTotalAssert);
    cJSON *jFreeAssert = cJSON_CreateNumber(PrettyDouble(GetGridTrade()->GetFreeAssert()));
    cJSON_AddItemToObject(jRoot, "剩余可用资产", jFreeAssert);
    cJSON *jYield = cJSON_CreateNumber(PrettyDouble(GetGridTrade()->Yield()));
    cJSON_AddItemToObject(jRoot, "收益", jYield);
    cJSON *jYieldRatio = cJSON_CreateNumber(PrettyDouble(GetGridTrade()->YieldRatio()));
    cJSON_AddItemToObject(jRoot, "收益率", jYieldRatio);
    cJSON *jCopies = cJSON_CreateNumber(GetGridTrade()->GetCopies());
    cJSON_AddItemToObject(jRoot, "总份数", jCopies);
    cJSON *jDealCount = cJSON_CreateNumber(GetGridTrade()->GetDealCount());
    cJSON_AddItemToObject(jRoot, "总成交次数", jDealCount);
    cJSON *jStandardStock = cJSON_CreateNumber(PrettyDouble(GetGridTrade()->GetStandardStock()));
    cJSON_AddItemToObject(jRoot, "基准股价", jStandardStock);
    cJSON *jStandardPostion = cJSON_CreateNumber(GetGridTrade()->GetStandardStockPostion());
    cJSON_AddItemToObject(jRoot, "基准股价网格序号", jStandardPostion);
    {
        cJSON *jDeals = cJSON_CreateArray();
        cJSON_AddItemToObject(jRoot, "成交细节", jDeals);
        std::unique_lock<std::recursive_mutex> lock(GetGridTrade()->GetMutex());
        for (size_t i = 0; i < GetGridTrade()->GetDealCount(); i++)
        {
            cJSON *jDeal = cJSON_CreateObject();
            cJSON *jDealTime = cJSON_CreateString(GetGridTrade()->GetDealAt(i).DealTime);
            cJSON_AddItemToObject(jDeal, "成交时间", jDealTime);
            cJSON *jSum = cJSON_CreateNumber(PrettyDouble(GetGridTrade()->GetDealAt(i).Sum));
            cJSON_AddItemToObject(jDeal, "成交额", jSum);
            cJSON *jStocks = cJSON_CreateNumber(PrettyDouble(GetGridTrade()->GetDealAt(i).Stocks));
            cJSON_AddItemToObject(jDeal, "成交价", jStocks);
            cJSON *jStockPostion = cJSON_CreateNumber(GetGridTrade()->GetGridPostion(GetGridTrade()->GetDealAt(i).Stocks));
            cJSON_AddItemToObject(jDeal, "网格序号", jStockPostion);
            cJSON *jTransationNo = cJSON_CreateNumber(GetGridTrade()->GetDealAt(i).TransationNo);
            cJSON_AddItemToObject(jDeal, "事务号", jTransationNo);
            cJSON *jCopies = cJSON_CreateNumber(GetGridTrade()->GetDealAt(i).Copies);
            cJSON_AddItemToObject(jDeal, "份数", jCopies);
            cJSON *jOper = cJSON_CreateString((GetGridTrade()->GetDealAt(i).Oper == OperMode::Buy) ? "买入" : "卖出");
            cJSON_AddItemToObject(jDeal, "操作类型", jOper);
            cJSON_AddItemToArray(jDeals, jDeal);
        }
    }
    std::string Res = cJSON_Print(jRoot);
    cJSON_Delete(jRoot);
    return Res;
}

std::string GetGridStat()
{
    cJSON *jRoot = cJSON_CreateObject();
    cJSON *jArray = cJSON_CreateArray();
    auto Grids = GetGridTrade()->GetStockGrids();
    for (size_t i = 0; i < Grids.size(); i++)
    {
        cJSON *Obj = cJSON_CreateObject();
        cJSON *jGridNo = cJSON_CreateNumber(i);
        cJSON_AddItemToObject(Obj, "网格号", jGridNo);
        cJSON *jGrid = cJSON_CreateNumber(PrettyDouble(Grids[i]));
        cJSON_AddItemToObject(Obj, "网格线", jGrid);
        cJSON_AddItemToArray(jArray, Obj);
    }
    cJSON_AddItemToObject(jRoot, "网格", jArray);
    std::string Res = cJSON_Print(jRoot);
    cJSON_Delete(jRoot);
    return Res;
}

std::string GetClosingPostionStat()
{
    cJSON *jRoot = cJSON_CreateObject();
    cJSON *jArray = cJSON_CreateArray();
    auto ClosingPostions = GePostionMgr()->CopyClosingPostions();
    size_t i = 0;
    for (auto &it : ClosingPostions)
    {
        cJSON *Obj = cJSON_CreateObject();
        cJSON *jTransNo = cJSON_CreateNumber(it.first);
        cJSON_AddItemToObject(Obj, "事务号", jTransNo);

        auto deal = GetGridTrade()->CopyDealAt(it.first);
        cJSON *JStock = cJSON_CreateNumber(PrettyDouble(deal.Stocks));
        cJSON_AddItemToObject(Obj, "股价", JStock);
        cJSON *jArray2 = cJSON_CreateArray();

        auto &ClosingPostion = it.second;
        for (size_t j = 0; j < ClosingPostion.size(); j++)
        {
            cJSON *Obj2 = cJSON_CreateObject();
            cJSON *jInfo = cJSON_CreateString(ClosingPostion[j].c_str());
            cJSON_AddItemToObject(Obj2, "明细", jInfo);
            cJSON_AddItemToArray(jArray2, Obj2);
        }
        cJSON_AddItemToObject(Obj, "平仓明细", jArray2);
        cJSON_AddItemToArray(jArray, Obj);
        i++;
    }
    cJSON_AddItemToObject(jRoot, "平仓", jArray);
    std::string Res = cJSON_Print(jRoot);
    cJSON_Delete(jRoot);
    return Res;
}

int main(int argc, char *argv[])
{
    REGTHREADINFO("main");

    if (argc < 2)
    {
        LOG_FATAL("启动参数错误, 示例: \n   grid_trade start xxx.json --前台启动\n   grid_trade startdaemon xxx.json --后台启动\n   grid_trade stop --停止进程");
        exit(-1);
    }
    if (argv[1] == std::string("startdaemon"))
    {
        if (access("/var/tmp/grid_trade/pid.file", F_OK) == 0)
        {
            LOG_ERROR("后台启动失败，进程似乎已经在运行了");
            exit(0);
        }
        LOG_WARN("后台启动");
        Daemon();

        // 在/var/tmp/grid_trade/pid.file 中记录进程号
        ShellExec("mkdir -p /var/tmp/grid_trade");
        int pid = getpid();
        std::string szBuf;
        szBuf += "echo ";
        szBuf += std::to_string(pid);
        szBuf += " > /var/tmp/grid_trade/pid.file";
        ShellExec(szBuf);
    }
    else if (argv[1] == std::string("start"))
    {
        LOG_WARN("前台启动模式");
    }
    else if (argv[1] == std::string("stop"))
    {
        if (access("/var/tmp/grid_trade/pid.file", F_OK) == 0)
        {
            LOG_WARN("向进程发送停止信号");
            ShellExec("cat /var/tmp/grid_trade/pid.file |xargs kill");
            ShellExec("rm -f /var/tmp/grid_trade/pid.file");
        }
        else
        {
            LOG_WARN("停止进程失败, 似乎没有在运行");
        }
        return 0;
    }
    else
    {
        LOG_FATAL("启动参数错误, 示例: \n   grid_trade start --前台启动\n   grid_trade startdaemon --后台启动\n   grid_trade stop --停止进程");
        exit(-1);
    }

    if (argc != 3)
    {
        LOG_FATAL("启动参数错误, 示例: \n   grid_trade start xxx.json --前台启动\n   grid_trade startdaemon xxx.json --后台启动\n   grid_trade stop --停止进程");
        exit(-1);
    }

    signal(SIGSEGV, sig_handler);
    signal(SIGINT, sig_handler);

    Log::GetInstance()->SetLogLevel(Log::LEVEL::INFO);
    LOG_ALARM("启动网格交易");

    szConfigPath = argv[2];
    LOG_WARN("读取配置文件 %s", szConfigPath.c_str());
    ReadConfig();

    // 初始化策略
    InitStrategy(TopStock, LowStock, InitAssert, GridWidth, WidthCopies);

    // 历史重演
    Replay();

    // 启动http服务端
    server.SetManageFunc("GetPostionStat", GetPostionStat);
    server.SetManageFunc("GetAssertStat", GetAssertStat);
    server.SetManageFunc("GetGridStat", GetGridStat);
    server.SetManageFunc("GetClosingPostionStat", GetClosingPostionStat);
    server.Init("", 9876);
    server.Start();

    // 启动股价查询
    InitReqStocks();
    client.Init("qt.gtimg.cn");
    client.Start();
    ReqStock();
    LOG_INFO("准备退出网格交易");

    server.Stop();
    client.Stop();
    LOG_ALARM("退出网格交易");
    return 0;
}