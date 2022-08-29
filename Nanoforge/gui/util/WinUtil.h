#pragma once
#include "common/Typedefs.h"
#include "common/String.h"
#include <optional>

struct HWND__;
using HWND = HWND__*;

void WinUtilInit(HWND mainWindowHandle); //Called once by Application class so the other functions can be called without passing hwnd to them
std::optional<string> OpenFile(const char* filter = nullptr);
std::optional<string> OpenFolder();
std::optional<string> SaveFile(const char* filter = nullptr);
int ShowMessageBox(const string& text, const string& caption, u32 type = 0, u32 icon = 0);
void ToastNotification(const string& text, const string& title = "");