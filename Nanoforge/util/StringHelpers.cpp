#include "StringHelpers.h"
#include <cstdlib>
#include <cstring>

std::unique_ptr<wchar_t[]> WidenCString(const char* c)
{
    const size_t cSize = strlen(c) + 1;
    auto wc = std::unique_ptr<wchar_t[]>(new wchar_t[cSize]);
    mbstowcs(wc.get(), c, cSize);

    return wc;
}