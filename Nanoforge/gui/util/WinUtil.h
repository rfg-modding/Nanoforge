#pragma once
#include "common/Typedefs.h"
#include <optional>
#include <ext/WindowsWrapper.h>

void WinUtilInit(HWND mainWindowHandle); //Called once by Application class so the other functions can be called without passing hwnd to them
std::optional<string> OpenFile(const char* filter = nullptr);
std::optional<string> OpenFolder();
std::optional<string> SaveFile(const char* filter = nullptr);
void ShowMessageBox(const string& text, const string& caption, u32 type = 0);