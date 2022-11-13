/*
 * Copyright (C) Alex Nekipelov (alex@nekipelov.net)
 * License: MIT
 */

#ifndef HTTPPARSER_REQUEST_H
#define HTTPPARSER_REQUEST_H

#include <string>
#include <vector>
#include <sstream>

namespace httpparser
{

struct Request {
    Request()
        : versionMajor(0), versionMinor(0), keepAlive(false)
    {}
    
    struct HeaderItem
    {
        std::string name;
        std::string value;
    };

    std::string method;
    std::string uri;
    int versionMajor;
    int versionMinor;
    std::vector<HeaderItem> headers;
    std::vector<char> content;
    bool keepAlive;
    std::string Empty = " ";

    std::string inspect() const
    {
        std::stringstream stream;
        stream << method << " " << uri << " HTTP/"
               << versionMajor << "." << versionMinor << "\n";

        for(std::vector<Request::HeaderItem>::const_iterator it = headers.begin();
            it != headers.end(); ++it)
        {
            stream << it->name << ": " << it->value << "\n";
        }

        std::string data(content.begin(), content.end());
        stream << data << "\n";
        stream << "+ keep-alive: " << keepAlive << "\n";;
        return stream.str();
    }

    std::string &operator[](std::string str)
    {
        for (auto &it : headers)
        {
            if (it.name == str)
            {
                return it.value;
            }
        }
        return Empty;
    }
    std::string GetStrContent()
    {
        std::string strContent;
        strContent.insert(strContent.begin(), content.begin(), content.end());
        return strContent;
    }
    void Reset()
    {
        method.clear();
        uri.clear();
        versionMajor = 0;
        versionMinor = 0;
        headers.clear();
        content.clear();
        keepAlive = false;
    }
};

} // namespace httpparser


#endif // HTTPPARSER_REQUEST_H
