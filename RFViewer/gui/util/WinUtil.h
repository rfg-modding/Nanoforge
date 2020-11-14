#pragma once
#include "common/Typedefs.h"
#include <optional>
#include <ext/WindowsWrapper.h>

std::optional<string> OpenFile(HWND hwnd, const string& filter, const char* title = "Open a file");
std::optional<string> OpenFolder(HWND hwnd, const char* title = "Select a folder");
std::optional<string> SaveFile(HWND hwnd, const string& filter, const char* title = "Save a file");