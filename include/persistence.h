#pragma once

#include <string>
#include <stdio.h>
#include <stdlib.h>

#include "Log.h"

namespace NSPersisTence
{
    struct PersistenceHead
    {
        bool IsValidTrans()
        {
            return MARK == 1234321;
        }

        uint32_t TransSize;
        uint32_t version;
        uint32_t FunctionID;
        uint32_t MARK = 1234321;
    };
};

class PersistenceReader
{
public:
    PersistenceReader() = default;
    ~PersistenceReader()
    {
        if (fp_ != nullptr)
        {
            fclose(fp_);
        }
        if (lpBuff_ != nullptr)
        {
            free(lpBuff_);
        }
    }

    void Open(const std::string &szPath)
    {
        if (fp_ != nullptr)
        {
            fclose(fp_);
        }
        CompleteTransPos_ = 0;

        fp_ = fopen64(szPath.c_str(), "rb+");
        if (fp_ == nullptr)
        {
            LOG_ERROR("打开持久化文件 %s 失败", szPath.c_str());
            throw std::runtime_error("打开持久化文件失败");
        }
        fseeko64(fp_, 0, SEEK_SET);
        return;
    }

    // 根据文件，按顺序读
    void *Read(uint32_t &Size)
    {
        if (fp_ == nullptr)
        {
            return nullptr;
        }

        fseeko64(fp_, 0, SEEK_END);
        if ((ftello64(fp_) - CompleteTransPos_) < (off64_t)sizeof(NSPersisTence::PersistenceHead))
        {
            return nullptr;
        }

        fseeko64(fp_, CompleteTransPos_, SEEK_SET);
        NSPersisTence::PersistenceHead head;
        fread(&head, sizeof(NSPersisTence::PersistenceHead), 1, fp_);
        if (!head.IsValidTrans())
        {
            LOG_FATAL("持久化数据有问题");
            throw std::runtime_error("持久化数据有问题");
        }

        fseeko64(fp_, 0, SEEK_END);
        if ((ftell(fp_) - CompleteTransPos_) >= head.TransSize)
        {
            fseeko64(fp_, CompleteTransPos_ + sizeof(NSPersisTence::PersistenceHead), SEEK_SET);
            auto lpTmp = realloc(lpBuff_, head.TransSize - sizeof(NSPersisTence::PersistenceHead));
            if (lpTmp == nullptr)
            {
                LOG_ERROR("内存申请失败 %u", head.TransSize - sizeof(NSPersisTence::PersistenceHead));
                throw std::runtime_error("内存申请失败");
            }
            lpBuff_ = (char *)lpTmp;
            fread(lpBuff_, head.TransSize - sizeof(NSPersisTence::PersistenceHead), 1, fp_);
            CompleteTransPos_ += head.TransSize;
            Size = head.TransSize - sizeof(NSPersisTence::PersistenceHead);
            return lpBuff_;
        }
        return nullptr;
    }

    // 截断文件，保证文件内容完整
    void Truncation()
    {
        if (fp_ == nullptr)
        {
            return;
        }
        ftruncate64(fileno(fp_), CompleteTransPos_);
        fseeko64(fp_, CompleteTransPos_, SEEK_SET);
    }

private:
    char *lpBuff_ = nullptr;
    FILE *fp_ = nullptr;
    off64_t CompleteTransPos_ = 0;
};

class PersistenceWriter
{
public:
    PersistenceWriter() = default;
    ~PersistenceWriter()
    {
        if (fp_ != nullptr)
        {
            fclose(fp_);
        }
    }

    void Open(const std::string &szPath)
    {
        if (fp_ != nullptr)
        {
            fclose(fp_);
        }

        fp_ = fopen64(szPath.c_str(), "ab+");
        if (fp_ == nullptr)
        {
            LOG_ERROR("打开持久化文件 %s 失败", szPath.c_str());
            throw std::runtime_error("打开持久化文件失败");
        }
        fseeko64(fp_, 0, SEEK_END);
        WritePos_ = ftell(fp_);
        return;
    }

    void Write(const void *szBuff, uint32_t Size)
    {
        if (fp_ == nullptr)
        {
            return;
        }
        fseeko64(fp_, WritePos_, SEEK_SET);
        NSPersisTence::PersistenceHead head;
        head.TransSize = sizeof(NSPersisTence::PersistenceHead) + Size;
        if (fwrite(&head, sizeof(NSPersisTence::PersistenceHead), 1, fp_) != 1)
        {
            LOG_ERROR("写持久化文件失败 %d %s", errno, strerror(errno));
            ftruncate64(fileno(fp_), WritePos_);
            fseeko64(fp_, WritePos_, SEEK_SET);
            return;
        }

        if (fwrite(szBuff, Size, 1, fp_) != 1)
        {
            LOG_ERROR("写持久化文件失败 %d %s", errno, strerror(errno));
            ftruncate64(fileno(fp_), WritePos_);
            fseeko64(fp_, WritePos_, SEEK_SET);
            return;
        }

        WritePos_ += head.TransSize;
        fflush(fp_);
    }

private:
    FILE *fp_ = nullptr;
    off64_t WritePos_ = 0;
};

