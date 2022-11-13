#pragma once

#include <string.h>

#include <string>
#include <map>

inline const char *HttpErrorMsg(uint32_t uErrorCode)
{
    switch (uErrorCode)
    {
    case 200:
        return "200 ok";
    
    default:
        return "404 not found";
    }
}



class HttpAssembler
{
public:
    HttpAssembler() = default;
    ~HttpAssembler() = default;

    void Reset()
    {
        Options_.clear();
        Body_.clear();
        Data_.clear();
    }

    void AddOption(const std::string &szKey, const std::string &szValue)
    {
        Options_[szKey] = szValue;
    }

    void AddBody(const std::string &szBody)
    {
        Body_ = szBody;
    }

    std::string &AssembleRsp(uint32_t ErrorCode = 200)
    {
        Data_ += "HTTP/1.1 ";
        Data_ += HttpErrorMsg(ErrorCode);
        Data_ += "\r\n";
        for (auto &it : Options_)
        {
            Data_ += it.first;
            Data_ += ": ";
            Data_ += it.second;
            Data_ += "\r\n";
        }
        Data_ += "\r\n";

        Data_ += Body_;

        return Data_;
    }

    std::string &GetData()
    {
        return Data_;
    }

    std::string &AssembleReq(const std::string &Path, const std::string &ReqType = "GET")
    {
        Data_ += ReqType;
        Data_ += " ";
        Data_ += Path;
        Data_ += " ";
        Data_ += " HTTP/1.1";
        Data_ += "\r\n";
        for (auto &it : Options_)
        {
            Data_ += it.first;
            Data_ += ": ";
            Data_ += it.second;
            Data_ += "\r\n";
        }
        Data_ += "\r\n";

        Data_ += Body_;

        return Data_;
    }

private:
    std::map<std::string, std::string> Options_;
    std::string Body_;
    std::string Data_;
};
