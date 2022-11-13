#include "format_time.h"
#include "shell_exec.h"
#include "Log.h"
#include <string>
#include <map>

#include <fstream>

void SendMail(const std::string &szTitle, const std::string &szMsg)
{
    std::string strBuff;

    strBuff += "echo '";
    strBuff += szMsg;
    strBuff += "' | mailx -s '";
    strBuff += szTitle;
    strBuff += "' zzhkikyou@163.com";
    ShellExec(strBuff);
}

int main()
{
    Log::GetInstance()->SetLogLevel(Log::LEVEL::DEBUG);

    int32_t i = 0;
    while (1)
    {
        std::string szBuff;
        szBuff += "这是第 [";
        szBuff += std::to_string(i);
        szBuff += "] 份通知";
        SendMail("通知", szBuff);
        i++;

        getchar();
    }
}