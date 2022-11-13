#include "format_time.h"
#include "shell_exec.h"
#include "Log.h"
#include <configor/json.hpp>

#include <string>
#include <map>
#include <tuple>
#include <fstream>
using namespace configor;


int main()
{
    Log::GetInstance()->SetLogLevel(Log::LEVEL::DEBUG);

    json::value j;
    j["number"] = 1;
    j["float"] = 1.5;
    j["string"] = "this is a string";
    j["boolean"] = true;
    j["user"]["id"] = 10;
    j["user"]["name"] = "Nomango";
    LOG_INFO("user name %s", j["user"]["name"].get<std::string>().c_str());
    j["user"]["name"] = "zzh";
    LOG_INFO("user name %s", j["user"]["name"].get<std::string>().c_str());
    j["char"] = json::array{"a", "b", "c"};

    if (j["user"]["passwd"].empty())
    {
        LOG_INFO("no passwd");
        //LOG_INFO("no passwd", j["user"]["passwd"].get<std::string>().c_str());
    }

    LOG_INFO("array size %lu", j["char"].size());
    LOG_INFO("array [1] %s", j["char"][1].get<std::string>().c_str());

    json::value j2 = json::object{
        {"null", nullptr},
        {"integer", 1},
        {"float", 1.3},
        {"boolean", true},
        {"string", "something"},
        {"array", json::array{1, 2}},
        {"object", json::object{
                       {"key", "value"},
                       {"key2", "value2"},
                   }},
    };

    std::string str = json::dump(j);

    LOG_INFO("\n%s", str.c_str());

    configor::json::value j3;
    std::vector<std::string> Jsons;
    std::vector<int> Params;
    for (auto &it : Params)
    {
        configor::json::value tmpj;
        tmpj["value"] = it;
        Jsons.push_back(json::dump(tmpj));
    }
    j3 = configor::json::array{{std::begin(Jsons), std::end(Jsons)}};
    std::string str2 = json::dump(j3);

    LOG_INFO("\n%s", str2.c_str());
}