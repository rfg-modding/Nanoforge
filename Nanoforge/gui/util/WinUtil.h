#pragma once
#include "common/Typedefs.h"
#include <optional>
#include <ext/WindowsWrapper.h>

void WinUtilInit(HWND mainWindowHandle); //Called once by Application class so the other functions can be called without passing hwnd to them
std::optional<string> OpenFile(const string& filter, const char* title = "Open a file");
std::optional<string> OpenFolder(const char* title = "Select a folder");
std::optional<string> SaveFile(const string& filter, const char* title = "Save a file");
void ShowMessageBox(const string& text, const string& caption, u32 type = 0);