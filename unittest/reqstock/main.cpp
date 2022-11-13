#include <string>
#include "tcp.h"
#include "format_time.h"
#include "encode_convert.h"
#include "httpparser/httpresponseparser.h"
#include "http_assemble.h"
#include <netdb.h>
#include <arpa/inet.h>

// v_s_sz002415="51~海康威视~002415~30.02~0.71~2.42~570378~170183~~2831.85~GP-A";
void ParseStock(std::string szContence)
{
    LOG_INFO("=======================================>");
    std::stringstream ss(szContence);
    std::string item;

    std::getline(ss, item, '"');

    for (size_t i = 0; i < 10; i++)
    {
        if (!std::getline(ss, item, '~'))
        {
            return;
        }

        switch (i)
        {
        case 0:
            LOG_INFO("交易所: %s", item.c_str());
            break;
        case 1:
            LOG_INFO("股票名字: %s", item.c_str());
            break;
        case 2:
            LOG_INFO("股票代码: %s", item.c_str());
            break;
        case 3:
            LOG_INFO("当前价格: %s", item.c_str());
            break;
        case 4:
            LOG_INFO("涨跌: %s", item.c_str());
            break;
        case 5:
            LOG_INFO("涨跌%: %s", item.c_str());
            break;
        case 6:
            LOG_INFO("成交量: %s", item.c_str());
            break;
        case 7:
            LOG_INFO("成交额: %s", item.c_str());
            break;
        case 8:
            // LOG_INFO("-: %s", item.c_str());
            break;
        case 9:
            LOG_INFO("总市值: %s", item.c_str());
            break;
        default:
            break;
        }
    }

    return;
}

thread_local httpparser::Response response;
thread_local httpparser::HttpResponseParser parser;
thread_local HttpAssembler httpAssem;

ClientConnection *lpConn = nullptr;
volatile bool bConnected = false;

void OnConnected()
{
    LOG_WARN("OnConnected fd:%d", lpConn->GetHandler()->GetFd());
    bConnected = true;
}
void OnDisconnected()
{
    LOG_WARN("OnDisconnected fd:%d", lpConn->GetHandler()->GetFd());
    bConnected = false;
}
void OnMessage(const void *lpData, uint32_t uLength)
{
    auto strContentType = response["Content-Type"];
    if (strContentType.find("GBK") != std::string::npos)
    {
        // GBK 需要转码
        auto StrGBKBody = response.GetStrContent();
        std::string StrUTFBody;
        GBK_2_UTF8(StrGBKBody, StrUTFBody);
        ParseStock(StrUTFBody);
    }
}

uint32_t OnRecvRawData(const void *lpData, uint32_t uLength)
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
        lpConn->Disconnect();
        uLength = 0;
        break;
    }

    return uLength;
}

int main()
{
    Log::GetInstance()->SetLogLevel(Log::LEVEL::DEBUG);
    LOG_WARN("http client Start");

    struct hostent* host = NULL;
    host = gethostbyname("qt.gtimg.cn");
    if (NULL == host)
    {
        LOG_ERROR("gethostbyname qt.gtimg.cn error");
        return 0;
    }

    char ip[64];
    inet_ntop(host->h_addrtype, host->h_addr, ip, 64);
    LOG_WARN("www.qt.gtimg.cn ip [%s]", ip);

    Client client;
    client.Start();

    lpConn = client.NewConnection(ip, 80, OnMessage, OnRecvRawData, OnConnected, OnDisconnected);

    lpConn->Connect();

    while (!lpConn->IsConnected())
    {
        LOG_WARN("等待客户端连接成功...");
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    LOG_WARN("客户端连接成功");

    httpAssem.AddOption("Accept-Language", "zh-cn;q=0.8,en-US;q=0.9");
    httpAssem.AddOption("Accept-Charset", "utf-8, iso-8859-1;q=0.5");
    httpAssem.AddOption("Host", "qt.gtimg.cn");
    auto &strParam = httpAssem.AssembleReq("/q=s_sz002415");
    // LOG_DEBUG("\n%s", strParam.c_str());

    while (1)
    {
        if (!lpConn->IsConnected())
        {
            LOG_DEBUG("连接已断开，即将重新连接");
            if (lpConn->Connect() != 0)
            {
                LOG_ERROR("重新连接失败，%d %s", errno, strerror(errno));
            }
            while (!lpConn->IsConnected())
            {
                LOG_WARN("等待客户端连接成功...");
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }

        lpConn->Send(strParam.data(), strParam.size());
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }

    getchar();
    client.Stop();
}