#pragma once

#include <string>
#include <stdlib.h>
#include <iconv.h>
#include <stdint.h>

inline int32_t encoding_convert(std::string const &src_str, std::string &dst_str,
                      std::string const &src_encoding, std::string const &dst_encoding)
{
    static thread_local iconv_t cd = 0;
    if (cd == 0)
    {
        if ((cd = iconv_open(dst_encoding.c_str(), src_encoding.c_str())) == 0)
        {
            return -1;
        }
    }

    std::size_t src_len = src_str.size();
    std::size_t dst_len = src_len * 2;
    dst_str.resize(dst_len);

    char *src_data = (char *)(src_str.data());
    char *dst_data = (char *)(dst_str.data());
    std::size_t left_len{dst_len};
    if (iconv(cd, &src_data, &src_len, &dst_data, &left_len) == (size_t)-1)
    {
        return -1;
    }
    //iconv_close(cd);
    dst_str.resize(dst_len - left_len);
    return 0;
}

#define GBK "gbk"
#define UTF8 "utf8"


#define GBK_2_UTF8(src_str, dst_str) encoding_convert(src_str, dst_str, GBK, UTF8)
#define UTF8_2_GBK(src_str, dst_str) encoding_convert(src_str, dst_str, UTF8, GBK)

