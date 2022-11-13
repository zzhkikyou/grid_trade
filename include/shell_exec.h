#pragma once

#include <stdio.h>
#include <string.h>
#include <string>
#include <stdint.h>
#include <exception>
#include <iostream>

inline std::string ShellExec(const std::string &szCmd)
{
    char buf_ps[1024];
    FILE *ptr = nullptr;
    if ((ptr = popen(szCmd.c_str(), "r")) == nullptr)
    {
        throw std::runtime_error("popen fail");
    }

    std::string szRes;
    while (fgets(buf_ps, sizeof(buf_ps), ptr) != nullptr)
    {
        szRes += buf_ps;
    }
    pclose(ptr);
    return szRes;
}
