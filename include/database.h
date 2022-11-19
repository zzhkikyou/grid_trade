#pragma once
#include <string>
#include <sqlite3.h>
#include <functional>
#include <map>
#include <vector>
#include <mutex>
#include "exception_wrap.h"

#define DATABASE_PRE_DEFINE \
    thread_local std::vector<std::map<std::string, std::string>> Database::Params_;

class Database
{
public:
    Database() = default;
    ~Database()
    {
        Close();
    }

    // 创建或打开数据库
    void Open(const std::string &strDatabase)
    {
        int32_t iRet = SQLITE_OK;
        Close();
        if ((iRet = sqlite3_open(strDatabase.c_str(), &lpDb_)) != SQLITE_OK)
        {
            THROW("sqlite3_open fail. %d %s", iRet, sqlite3_errmsg(lpDb_));
            Close();
        }
    }

    void Close()
    {
        if (lpDb_)
        {
            sqlite3_close(lpDb_);
        }
        lpDb_ = nullptr;
    }

    // 开始事务
    void Begin()
    {
        Exec("begin transaction;");
    }
    // 提交事务
    void Commit()
    {
        Exec("commit transaction;");
    }
    // 回滚事务
    void Rollback()
    {
        Exec("rollback transaction;");
    }

    void Exec(const std::string &Sql, int (*callback)(void *, int, char **, char **) = nullptr, void *lpArgs = nullptr)
    {
        if (lpDb_ == nullptr)
        {
            THROW("Exec fail. db is null");
        }
        
        std::unique_lock<std::mutex> lock(mutex_);
        Params_.clear();
        int32_t iRet = SQLITE_OK;
        char *lpErrMsg = nullptr;

        // cppcheck-suppress constParameter
        auto MyCallback = [](void *lpUser, int32_t columns, char **value, char **key) -> int32_t
        {
            std::map<std::string, std::string> param;
            for (int32_t i = 0; i < columns; i++)
            {
                param[key[i]] = (value[i] ? value[i] : "");
            }
            Params_.push_back(param);
            return 0;
        };

        if ((iRet = sqlite3_exec(lpDb_, Sql.c_str(), callback ? callback : MyCallback, lpArgs ? lpArgs : this, &lpErrMsg)) != SQLITE_OK)
        {
            THROW("Exec fail. %d %s", iRet, lpErrMsg);
            sqlite3_free(lpErrMsg);
        }
    }

    // 获取查询结果
    static std::vector<std::map<std::string, std::string>> &GetRes()
    {
        return Params_;
    }

private:
    std::mutex mutex_;
    sqlite3 *lpDb_ = nullptr;
    std::function<int32_t(int , char **, char **)> fnCallback_;

    static thread_local std::vector<std::map<std::string, std::string>> Params_;
};