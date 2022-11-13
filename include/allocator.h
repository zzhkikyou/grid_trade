#pragma once
#include <unistd.h>
#include <stdlib.h>
#include <exception>
#include <stdint.h>
#include <string.h>
#include <iostream>
#include <vector>

class allocator
{
public: 
    allocator() = default;
    ~allocator(){}

    void *AllocMemContinuous(uint32_t uSize)
    {
        Realloc(usage_ + uSize);
        return buffer_.data() + usage_;
    }

    void ConfirmUsage(uint32_t uSize)
    {
        usage_ += uSize;
    }

    void RemoveUsage(uint32_t uSize)
    {
        usage_ -= uSize;
        MoveForward(uSize, usage_);
    }

    void *GetBuffer() const
    {
        return (void *)(buffer_.data());
    }

    uint32_t GetUsage() const
    {
        return usage_;
    }

    void Clear()
    {
        usage_ = 0;
    }

private:
    void MoveForward(uint32_t uStartPos, uint32_t uLength)
    {
        memmove(buffer_.data(), buffer_.data() + uStartPos, uLength);
        buffer_.resize(uLength);
    }

    void Realloc(uint32_t uSize)
    {
        if ((uSize == 0) || (buffer_.size() >= uSize))
        {
            return ;
        }
        buffer_.resize(uSize + 4096);
    }

private:
    std::vector<uint8_t> buffer_;
    uint32_t usage_ = 0;
};
