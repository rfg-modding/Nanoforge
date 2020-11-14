#pragma once
#include <memory>

//Convert const char* to wchar_t*. Source: https://stackoverflow.com/a/8032108
std::unique_ptr<wchar_t[]> WidenCString(const char* c);
