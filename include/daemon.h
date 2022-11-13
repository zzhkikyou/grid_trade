#pragma once

#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <exception>
#include <iostream>


inline void Daemon()
{
    int32_t fd = -1;
    switch (fork())
    {
    case -1:
        return;
    case 0:
        break;
    default:
        exit(0);
    }

    // 只有子进程流程才能到这
    if (setsid() == -1)
    {
        throw std::runtime_error("setsid fail");
    }
    umask(0); // 设置为0，不要让他来限制文件权限，以免引起混乱

    fd = open("/dev/null", O_RDWR); // 打开黑洞设备（以读写方式打开）
    if (fd == -1)
    {
        throw std::runtime_error("open fail");
    }

    if (dup2(fd, STDIN_FILENO) == -1)
    { // 先关闭STDIN_FILENO（这是规矩，已经打开的描述符，
      // 改动之前先关闭），类似于指针指向null，让/dev/null成为标准输入
        throw std::runtime_error("dup2 fail");
    }

    // 控制了界面打印
    // if (dup2(fd, STDOUT_FILENO) == -1)
    // { // 先关闭STDOUT_FILENO，类似于指针指向null，让/dev/null成为标准输出
    //     throw std::runtime_error("dup2 fail");
    // }

    if (fd > STDERR_FILENO)
    {
        if (close(fd) == -1)
        {
            throw std::runtime_error("close fail");
        }
    }
}

