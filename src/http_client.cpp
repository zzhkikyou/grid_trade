#include "common.h"

thread_local httpparser::Response HttpClient::response;
thread_local httpparser::HttpResponseParser HttpClient::parser;


// v_s_sz002415="51~海康威视~002415~30.02~0.71~2.42~570378~170183~~2831.85~GP-A";
double ParseStock(std::string szContence)
{
    double Stocks = 0.0;
    LOG_TRACE("=======================================>");
    std::stringstream ss(szContence);
    std::string item;

    std::getline(ss, item, '"');

    for (size_t i = 0; i < 10; i++)
    {
        if (!std::getline(ss, item, '~'))
        {
            return 0.0;
        }

        switch (i)
        {
        case 0:
            LOG_TRACE("交易所: %s", item.c_str());
            break;
        case 1:
            LOG_TRACE("股票名字: %s", item.c_str());
            break;
        case 2:
            LOG_TRACE("股票代码: %s", item.c_str());
            break;
        case 3:
            LOG_TRACE("当前价格: %s", item.c_str());
            Stocks = std::stold(item);
            break;
        case 4:
            LOG_TRACE("涨跌: %s", item.c_str());
            break;
        case 5:
            LOG_TRACE("涨跌%: %s", item.c_str());
            break;
        case 6:
            LOG_TRACE("成交量: %s", item.c_str());
            break;
        case 7:
            LOG_TRACE("成交额: %s", item.c_str());
            break;
        case 8:
            // LOG_TRACE("-: %s", item.c_str());
            break;
        case 9:
            LOG_TRACE("总市值: %s", item.c_str());
            break;
        default:
            break;
        }
    }

    return Stocks;
}

void HttpClient::Init(const std::string &szDomain)
{
    client_ = new Client();

    struct hostent *host = nullptr;
    if ((host = gethostbyname(szDomain.c_str())) == nullptr)
    {
        LOG_ERROR("gethostbyname %s error", szDomain.c_str());
        return;
    }

    char ip[64];
    inet_ntop(host->h_addrtype, host->h_addr, ip, 64);
    LOG_WARN("%s ip [%s]", szDomain.c_str(), ip);
    Ip_ = ip;
}

void HttpClient::Start()
{
    client_->Start();
    lpConn_ = client_->NewConnection(Ip_, 80, std::bind(&HttpClient::OnMessage, this, std::placeholders::_1, std::placeholders::_2),
                                     std::bind(&HttpClient::OnRecvRawData, this, std::placeholders::_1, std::placeholders::_2),
                                     std::bind(&HttpClient::OnConnected, this),
                                     std::bind(&HttpClient::OnDisconnected, this));
    lpConn_->Connect();
}

void HttpClient::Stop()
{
    if (client_)
    {
        client_->DeleteConnection(lpConn_);
        lpConn_ = nullptr;
    }
}

void HttpClient::RequestStock(const std::string &szMsg)
{
    while (!IsTerminated() && !lpConn_->IsConnected())
    {
        LOG_WARN("等待客户端连接成功...");
        lpConn_->Connect();
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    lpConn_->Send(szMsg.data(), szMsg.size());
}

void HttpClient::OnConnected()
{
    LOG_WARN("连接建立 fd:%d", lpConn_->GetHandler()->GetFd());
}
void HttpClient::OnDisconnected()
{
    LOG_WARN("连接断开 fd:%d", lpConn_->GetHandler()->GetFd());
}
void HttpClient::OnMessage(const void *lpData, uint32_t uLength)
{
    auto strContentType = response["Content-Type"];
    if (strContentType.find("GBK") != std::string::npos)
    {
        // GBK 需要转码
        auto StrGBKBody = response.GetStrContent();
        std::string StrUTFBody;
        GBK_2_UTF8(StrGBKBody, StrUTFBody);
        auto Stocks = ParseStock(StrUTFBody);
        if (Stocks != 0.0)
        {
            int32_t Hour, Min, DayInWeek;
            bool bDealTime = IsDealTime(Hour, Min, DayInWeek);
            if (bDealTime)
            {
                GetGridTrade()->Update(Stocks);
            }
            else
            {
                GetGridTrade()->UpdateClosed(Stocks); // 更新收盘价
            }
        }
    }
}
uint32_t HttpClient::OnRecvRawData(const void *lpData, uint32_t uLength)
{
    httpparser::HttpResponseParser::ParseResult res = parser.parse(response, (char *)lpData, (char *)lpData + uLength, uLength);
    switch (res)
    {
    case httpparser::HttpResponseParser::ParsingCompleted:
        break;
    case httpparser::HttpResponseParser::ParsingIncompleted:
        uLength = 0;
        break;
    case httpparser::HttpResponseParser::ParsingError:
        LOG_ERROR("http 协议解析失败, 即将主动断开连接");
        LOG_TRACE("\n%s", (char *)lpData);
        lpConn_->Disconnect();
        uLength = 0;
        break;
    }

    return uLength;
}
