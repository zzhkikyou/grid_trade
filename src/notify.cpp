#include "notify.h"

void notify::notify_zzh(uint64_t identify, const std::string &szTitle, const std::string &szMsg)
{
    if ((Lastidentify != 0) && (Lastidentify == identify))
    {
        return;
    }
    Lastidentify = identify;

    std::string strBuff;

    strBuff += "echo '";
    strBuff += "当前时间为: ";
    strBuff += GetLocalTime();
    strBuff += "\n";
    strBuff += szMsg;
    strBuff += "' | mailx -s '";
    strBuff += szTitle;
    strBuff += "' zzhkikyou@163.com";

    LOG_WARN("发送通知到 zzh: \n%s", szMsg.c_str());
    ShellExec(strBuff);
}