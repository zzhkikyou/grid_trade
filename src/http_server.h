#ifndef __HTTP_SERVER_H_
#define __HTTP_SERVER_H_

#include "common.h"

class HttpServer
{
public:
    HttpServer() = default;
    ~HttpServer() { delete server_; };

    void Init(const std::string &Ip, uint32_t Port);
    void Start();
    void Stop();
    void SetManageFunc(const std::string &FuncName, const std::function<std::string()> &fnManage);

private:
    void OnAccepted(ServerConnection *lpConn);
    void OnDisconnected(ServerConnection *lpConn);
    void OnMessage(ServerConnection *lpConn, const void *lpData, uint32_t uLength);
    uint32_t OnRecvRawData(ServerConnection *lpConn, const void *lpData, uint32_t uLength);

    void OnMessageData(ServerConnection *lpConn, const void *lpData, uint32_t uLength);
    void OnMessageFile(ServerConnection *lpConn, const void *lpData, uint32_t uLength);

    void ComfirmOper(std::map<std::string, std::string> &Params, std::string &Uri, std::string &szBody, uint32_t &ErrorCode);
    void ManageFunc(std::map<std::string, std::string> &Params, std::string &Uri, std::string &szBody, uint32_t &ErrorCode);

private:
    Server *server_ = nullptr;
    std::string Ip_;
    uint32_t Port_;
    std::map<std::string, std::function<std::string()>> fnManages;

private:
    static thread_local httpparser::Request request;
    static thread_local httpparser::HttpRequestParser parser;
    static thread_local HttpAssembler httpAssem;
};

#endif