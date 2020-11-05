#pragma once

//Convert const char* to wchar_t*. Source: https://stackoverflow.com/a/8032108. Caller must free wchar_t* after use
const wchar_t* WidenCString(const char* c);