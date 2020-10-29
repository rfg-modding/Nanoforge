#include "StringHelpers.h"
#include <cstdlib>
#include <cstring>

const wchar_t* WidenCString(const char* c)
{
    const size_t cSize = strlen(c) + 1;
    wchar_t* wc = new wchar_t[cSize];
    mbstowcs(wc, c, cSize);

    return wc;
}