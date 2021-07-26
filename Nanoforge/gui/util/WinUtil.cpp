#include "WinUtil.h"
#include <nfd.h>
#include <sstream>
#include <commdlg.h>
#include <ShlObj.h>
#include <filesystem>

//Main window handle. Used by the functions below.
HWND hwnd = NULL;

//OpenFile and SaveFile are based on https://github.com/TheCherno/Hazel/blob/master/Hazel/src/Platform/Windows/WindowsPlatformUtils.cpp

void WinUtilInit(HWND mainWindowHandle)
{
	hwnd = hwnd;
}

std::optional<string> OpenFile(const char* filter)
{
	char* out = nullptr;
	nfdresult_t result = NFD_OpenDialog(filter, nullptr, &out);
	if (result == NFD_OKAY)
		return string(out);
	else
		return {};
}

std::optional<string> OpenFolder()
{
	char* out = nullptr;
	nfdresult_t result = NFD_PickFolder(nullptr, &out);
	if (result == NFD_OKAY)
		return string(out);
	else
		return {};
}

std::optional<string> SaveFile(const char* filter)
{
	char* out = nullptr;
	nfdresult_t result = NFD_SaveDialog(filter, nullptr, &out);
	if (result == NFD_OKAY)
		return string(out);
	else
		return {};
}

void ShowMessageBox(const string& text, const string& caption, u32 type)
{
	MessageBoxA(hwnd, text.c_str(), caption.c_str(), type);
}