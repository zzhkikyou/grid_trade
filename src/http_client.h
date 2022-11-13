#ifndef __HTTP_CLIENT_H_
#define __HTTP_CLIENT_H_

#include "common.h"

class HttpClient
{
public:
    HttpClient() = default;
    ~HttpClient() { delete client_; }

    void Init(const std::string &szDomain);

    void Start();

    void Stop();

    void RequestStock(const std::string &szMsg);

private:
    void OnConnected();
    void OnDisconnected();
    void OnMessage(const void *lpData, uint32_t uLength);
    uint32_t OnRecvRawData(const void *lpData, uint32_t uLength);

private:
    Client *client_ = nullptr;
    std::string szDomain_;
    std::string Ip_;

private:
    static thread_local httpparser::Response response;
    static thread_local httpparser::HttpResponseParser parser;

    ClientConnection *lpConn_ = nullptr;
};

#endif