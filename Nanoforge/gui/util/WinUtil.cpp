#include "WinUtil.h"
#include <filesystem>
#include <ext/WindowsWrapper.h>
#pragma warning(disable:4996)
#pragma warning(disable:26812) //Disable library warnings
#include <nfd.h>
#pragma warning(default:26812)
#pragma warning(default:4996)

//Main window handle. Used by the functions below.
HWND hwnd = NULL;

void WinUtilInit(HWND mainWindowHandle)
{
	hwnd = mainWindowHandle;
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