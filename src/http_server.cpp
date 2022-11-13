#include "common.h"
#include "http_server.h"

thread_local httpparser::Request HttpServer::request;
thread_local httpparser::HttpRequestParser HttpServer::parser;
thread_local HttpAssembler HttpServer::httpAssem;

void HttpServer::Init(const std::string &Ip, uint32_t Port)
{
    server_ = new Server(Ip, Port, 10240);
    server_->SetfnOnAccepted(std::bind(&HttpServer::OnAccepted, this, std::placeholders::_1));
    server_->SetfnOnDisconnected(std::bind(&HttpServer::OnDisconnected, this, std::placeholders::_1));
    server_->SetfnOnMessage(std::bind(&HttpServer::OnMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    server_->SetfnOnRecvRawData(std::bind(&HttpServer::OnRecvRawData, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}
void HttpServer::Start()
{
    server_->Start();
}
void HttpServer::Stop()
{
    if (server_)
    {
        server_->Stop();
    }   
}

void HttpServer::OnAccepted(ServerConnection *lpConn)
{
    LOG_WARN("OnAccepted fd:%d %s:%d", lpConn->GetHandler()->GetFd(), lpConn->GetHandler()->GetPeerIp().c_str(), lpConn->GetHandler()->GetPeerPort());
}
void HttpServer::OnDisconnected(ServerConnection *lpConn)
{
    LOG_WARN("OnDisconnected fd:%d %s:%d", lpConn->GetHandler()->GetFd(), lpConn->GetHandler()->GetPeerIp().c_str(), lpConn->GetHandler()->GetPeerPort());
}

// name=111&passwd=11
std::map<std::string, std::string> ParamsParse(std::string &str)
{
    std::map<std::string, std::string> Params;
    std::stringstream ss(str);
    std::string item;
    while (std::getline(ss, item, '&'))
    {
        std::string key, value;
        std::stringstream ss2(item);
        if (!std::getline(ss2, key, '='))
            continue;
        if (!std::getline(ss2, value, '='))
            continue;
        Params[key] = value;
    }
    return Params;
}

void HttpServer::ComfirmOper(std::map<std::string, std::string> &Params, std::string &Uri, std::string &szBody, uint32_t &ErrorCode)
{
    double Stock = std::stod(Params["stocks"]);
    uint32_t copies = (uint32_t)std::stoi(Params["coipes"]);
    double sum = std::stod(Params["sum"]);

    if (Uri == "/comfirmbuy")
    {
        LOG_WARN("收到确认买入操作事件, 买入股价 %0.3lf, 买入份数 %u, 总价 %0.3lf", Stock, copies, sum);
        auto szTime = GetLocalTime();
        auto TransNo = GetGridTrade()->ComfirmBuy(Stock, copies, sum, szTime.c_str());
        GePostionMgr()->ConfirmBuy(TransNo, Stock, copies, szTime);
        DealInfo Info(TransNo, Stock, copies, sum, OperMode::Buy, szTime);
        GePersistenceWriter()->Write(&Info, sizeof(DealInfo));
        ErrorCode = 200;
        szBody = "success";
    }
    else if (Uri == "/comfirmsale")
    {
        LOG_WARN("收到确认卖出操作事件, 卖出股价 %0.3lf, 买入份数 %u 总价 %0.3lf", Stock, copies, sum);
        auto szTime = GetLocalTime();
        auto TransNo = GetGridTrade()->ComfirmSale(Stock, copies, sum, szTime.c_str());
        GePostionMgr()->ConfirmSale(TransNo, Stock, copies, szTime);
        DealInfo Info(TransNo, Stock, copies, sum, OperMode::Sale, szTime);
        GePersistenceWriter()->Write(&Info, sizeof(DealInfo));
        ErrorCode = 200;
        szBody = "success";
    }
    else
    {
        LOG_WARN("收到未知操作事件 %s", request.uri.c_str());
    }
}

void HttpServer::ManageFunc(std::map<std::string, std::string> &Params, std::string &Uri, std::string &szBody, uint32_t &ErrorCode)
{
    if (fnManages.count(Params["function"].c_str()) != 0)
    {
        szBody = fnManages[Params["function"].c_str()]();
        ErrorCode = 200;
    }
    else
    {
        LOG_WARN("收到未知管理功能 [%s]", Params["function"].c_str());
    }
}

void HttpServer::OnMessageData(ServerConnection *lpConn, const void *lpData, uint32_t uLength)
{
    std::string szBody("fail");
    uint32_t ErrorCode = 404;
    httpAssem.Reset();
    auto pos = request.GetStrContent().find(":"); 
    std::string strDataType(request.GetStrContent(), 0, pos);
    if (strDataType == "params")
    {
        std::string strParam(request.GetStrContent(), pos + 1);
        if (strParam.back() != '\0')
        {
            strParam += '\0';
        }

        auto Params = ParamsParse(strParam);

        if (request.uri == "/comfirmbuy" || request.uri == "/comfirmsale")
        {
            ComfirmOper(Params, request.uri, szBody, ErrorCode);
        }
        else if (request.uri == "/manager")
        {
            ManageFunc(Params, request.uri, szBody, ErrorCode);
        }
        else
        {
            LOG_WARN("收到未知操作事件 %s", request.uri.c_str());
        }
    }
    httpAssem.AddOption("Content-Length", std::to_string(szBody.size()));
    httpAssem.AddBody(szBody);
    httpAssem.AssembleRsp(ErrorCode);
    LOG_DEBUG("\n%s", httpAssem.GetData().c_str());
    lpConn->Send(httpAssem.GetData().data(), httpAssem.GetData().size());
}

void HttpServer::OnMessageFile(ServerConnection *lpConn, const void *lpData, uint32_t uLength)
{
    httpAssem.Reset();
    std::string szBody;
    LOG_INFO("uri: %s", request.uri.c_str());
    std::string szPath(request.uri, 1);
    szPath += ".html";
    szPath = "html/" + szPath;

    std::ifstream infile;
    infile.open(szPath, std::ios::out | std::ios::in | std::ios::binary);

    if (!infile.is_open())
    {
        httpAssem.AddOption("Date", GetLocalTime());
        httpAssem.AddOption("Content-Type", "text/html; charset=UTF-8");
        httpAssem.AddOption("Content-Length", "0");
        httpAssem.AssembleRsp(404);
    }
    else
    {
        std::string line;
        while (getline(infile, line))
        {
            szBody += line;
        }

        httpAssem.AddOption("Date", GetLocalTime());
        httpAssem.AddOption("Content-Type", "text/html; charset=UTF-8");
        httpAssem.AddOption("Content-Length", std::to_string(szBody.size()));
        httpAssem.AddBody(szBody);
        httpAssem.AssembleRsp();
    }
    infile.close();
    LOG_DEBUG("\n%s", httpAssem.GetData().c_str());
    lpConn->Send(httpAssem.GetData().data(), httpAssem.GetData().size());
}

void HttpServer::SetManageFunc(const std::string &FuncName, const std::function<std::string()> &fnManage)
{
    fnManages[FuncName] = fnManage;
}

void HttpServer::OnMessage(ServerConnection *lpConn, const void *lpData, uint32_t uLength)
{
    auto pos = request.GetStrContent().find(":"); // 判断请求的是页面还是数据，带参数的就是请求数据，不带参数就是请求页面  params:name=zzh&passwd=1
    if (pos != std::string::npos)
    {
        OnMessageData(lpConn, lpData, uLength);
    }
    else
    {
        OnMessageFile(lpConn, lpData, uLength);
    }
}
uint32_t HttpServer::OnRecvRawData(ServerConnection *lpConn, const void *lpData, uint32_t uLength)
{
    httpparser::HttpRequestParser::ParseResult res = parser.parse(request, (char *)lpData, (char *)lpData + uLength, uLength);
    switch (res)
    {
    case httpparser::HttpRequestParser::ParsingCompleted:
        break;
    case httpparser::HttpRequestParser::ParsingIncompleted:
        uLength = 0;
        break;
    case httpparser::HttpRequestParser::ParsingError:
        LOG_WARN("http 协议解析失败");
        // LOG_ERROR("http 协议解析失败, 即将主动断开连接");
        // LOG_TRACE("\n%s", (char *)lpData);
        // lpConn->Disconnect();
        uLength = 0;
        break;
    }
    LOG_DEBUG("\n%s", request.inspect().c_str());

    return uLength;
}