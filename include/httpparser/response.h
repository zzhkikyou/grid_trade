/*
 * Copyright (C) Alex Nekipelov (alex@nekipelov.net)
 * License: MIT
 */

#ifndef HTTPPARSER_RESPONSE_H
#define HTTPPARSER_RESPONSE_H

#include <string>
#include <vector>
#include <sstream>
#include <map>

namespace httpparser
{

struct Response {
    Response()
        : versionMajor(0), versionMinor(0), keepAlive(false), statusCode(0)
    {}
    
    struct HeaderItem
    {
        std::string name;
        std::string value;
    };

    int versionMajor;
    int versionMinor;
    std::vector<HeaderItem> headers;
    std::vector<char> content;
    bool keepAlive;
    unsigned int statusCode;
    std::string status;
    std::string Empty = " ";

    std::string inspect() const
    {
        std::stringstream stream;
        stream << "HTTP/" << versionMajor << "." << versionMinor
               << " " << statusCode << " " << status << "\n";

        for(std::vector<Response::HeaderItem>::const_iterator it = headers.begin();
            it != headers.end(); ++it)
        {
            stream << it->name << ": " << it->value << "\n";
        }

        std::string data(content.begin(), content.end());
        stream << data << "\n";
        return stream.str();
    }

    std::string GetStrContent()
    {
        std::string strContent;
        strContent.resize(content.size() + 1);
        for (size_t i = 0; i < content.size(); i++)
        {
            strContent[i] = content[i];
        }
        strContent[content.size()] = '\0';
        return strContent;
    }

    std::string &operator[](std::string str)
    {
        for (auto &it: headers)
        {
            if (it.name == str)
            {
                return it.value;
            }
        }
        return Empty;
    }

    void Reset()
    {
        versionMajor = 0;
        versionMinor = 0;
        headers.clear();
        content.clear();
        keepAlive = false;
        statusCode = 0;
        status.clear();
    }
};

} // namespace httpparser

#endif // HTTPPARSER_RESPONSE_H

