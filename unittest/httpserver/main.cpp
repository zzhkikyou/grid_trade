#include "tcp.h"
#include "httpparser/httprequestparser.h"
#include "http_assemble.h"
#include "format_time.h"
#include <fstream>
#include <string>
#include <map>

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

thread_local httpparser::Request request;
thread_local httpparser::HttpRequestParser parser;
thread_local HttpAssembler httpAssem;

void OnConnected(ServerConnection * lpConn)
{
        LOG_WARN("OnConnected fd:%d %s:%d", lpConn->GetHandler()->GetFd(), lpConn->GetHandler()->GetPeerIp().c_str(), lpConn->GetHandler()->GetPeerPort());
}
void OnDisconnected(ServerConnection *lpConn)
{
    LOG_WARN("OnDisconnected fd:%d %s:%d", lpConn->GetHandler()->GetFd(), lpConn->GetHandler()->GetPeerIp().c_str(), lpConn->GetHandler()->GetPeerPort());
}
void OnMessage(ServerConnection *lpConn, const void *lpData, uint32_t uLength)
{
    httpAssem.Reset();
    std::string szBody;

    LOG_INFO("uri: %s", request.uri.c_str());
    LOG_INFO("body: %s", request.GetStrContent().c_str());
    auto pos = request.GetStrContent().find(":"); // 判断请求的是页面还是数据  params:name=zzh&passwd=1
    if (pos != std::string::npos) 
    {
        std::string strDataType(request.GetStrContent(), 0, pos);
        if (strDataType == "params")
        {
            bool bLoginFail = true;
            std::string strParam(request.GetStrContent(), pos + 1);
            auto Params = ParamsParse(strParam);
            printf("Params [%s] [%s] [%s]\n", strParam.c_str(), Params["name"].c_str(), Params["passwd"].c_str());

            if ((Params["name"] == "zzh"))
            {
                if (Params["passwd"][0] == '1')
                {
                    bLoginFail = false;
                    LOG_INFO("张宗豪 登陆成功 %d", bLoginFail);
                }
                else
                {
                    bLoginFail = true;
                    LOG_INFO("张宗豪 登陆失败 %d size[%u] [%s]", bLoginFail, Params["passwd"].size(), Params["passwd"].c_str());
                }
            }
            httpAssem.AddOption("Date", GetLocalTime());
            httpAssem.AddOption("Content-Type", "text/plain; charset=UTF-8");

            if (bLoginFail)
            {
                szBody = "loginfail";
                httpAssem.AddOption("Content-Length", std::to_string(szBody.size()));
                httpAssem.AddBody(szBody);
                httpAssem.AssembleRsp();
            }
            else
            {
                szBody = "loginsuccess";
                httpAssem.AddOption("Content-Length", std::to_string(szBody.size()));
                httpAssem.AddBody(szBody);
                httpAssem.AssembleRsp();
            }
        }
    }
    else
    {
        std::string szPath(request.uri, 1);
        szPath += ".html";

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
    }

    LOG_DEBUG("\n%s", httpAssem.GetData().c_str());
    lpConn->Send(httpAssem.GetData().data(), httpAssem.GetData().size());
}

uint32_t OnRecvRawData(ServerConnection *lpConn, const void *lpData, uint32_t uLength)
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
        LOG_ERROR("http 协议解析失败, 即将主动断开连接");
        LOG_TRACE("\n%s", (char *)lpData);
        lpConn->Disconnect();
        uLength = 0;
        break;
    }
    LOG_DEBUG("\n%s", request.inspect().c_str());

    return uLength;
}

int main()
{
    Log::GetInstance()->SetLogLevel(Log::LEVEL::DEBUG);
    LOG_WARN("http server Start");

    Server server("", 9876, 10240);

    server.SetfnOnAccepted(OnConnected);
    server.SetfnOnDisconnected(OnDisconnected);
    server.SetfnOnMessage(OnMessage);
    server.SetfnOnRecvRawData(OnRecvRawData);

    server.Start();

    getchar();
    server.Stop();
}