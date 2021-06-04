#include "WinUtil.h"
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

std::optional<string> OpenFile(const string& filter, const char* title)
{
	//Get directory of exe and set that as the initial dir
	string workingDirectory = std::filesystem::absolute("./").string();

	//Setup open file dialog
	OPENFILENAMEA dialogConfig;
	CHAR szFile[MAX_PATH] = { 0 }; //Setup temporary output buffer
	ZeroMemory(&dialogConfig, sizeof(OPENFILENAMEA));
	dialogConfig.lStructSize = sizeof(OPENFILENAMEA);
	dialogConfig.hwndOwner = hwnd;
	dialogConfig.lpstrFile = szFile;
	dialogConfig.nMaxFile = sizeof(szFile);
	dialogConfig.lpstrFilter = filter.c_str();
	dialogConfig.lpstrTitle = title;
	dialogConfig.nFilterIndex = 1;
	dialogConfig.lpstrInitialDir = workingDirectory.c_str();
	dialogConfig.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
	
	//Run open file dialog and wait for output
	if (GetOpenFileName(&dialogConfig) == TRUE)
		return dialogConfig.lpstrFile; //Return selected files path

	//If no file opened return empty
	return {};
}

std::optional<string> OpenFolder(const char* title)
{
	//Setup folder browser
	BROWSEINFOA browseInfo;
	ZeroMemory(&browseInfo, sizeof(BROWSEINFOA));
	browseInfo.hwndOwner = hwnd;
	browseInfo.lpszTitle = title;
	browseInfo.ulFlags = 0;

	//Open folder browser and wait for return
	PIDLIST_ABSOLUTE pidl = SHBrowseForFolder(&browseInfo);
	if (pidl == NULL)
		return {};

	//Convert return value to folder path
	CHAR szPath[MAX_PATH] = { 0 }; //Setup temporary output buffer
	BOOL result = SHGetPathFromIDList(pidl, szPath);
	if (result == FALSE)
		return {}; //Return empty if failed to get path

	//Return path if successfully opened folder and got it's path
	return string(szPath);
}

std::optional<string> SaveFile(const string& filter, const char* title)
{
	//Get directory of exe and set that as the initial dir
	string workingDirectory = std::filesystem::absolute("./").string();

	//Setup save file dialog
	OPENFILENAMEA dialogConfig;
	CHAR szFile[1024] = { 0 }; //Setup temporary output buffer
	ZeroMemory(&dialogConfig, sizeof(OPENFILENAMEA));
	dialogConfig.lStructSize = sizeof(OPENFILENAMEA);
	dialogConfig.hwndOwner = hwnd;
	dialogConfig.lpstrFile = szFile;
	dialogConfig.nMaxFile = sizeof(szFile);
	dialogConfig.lpstrFilter = filter.c_str();
	dialogConfig.lpstrTitle = title;
	dialogConfig.nFilterIndex = 1;
	dialogConfig.lpstrInitialDir = workingDirectory.c_str();
	dialogConfig.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

	//Sets the default extension by extracting it from the filter
	dialogConfig.lpstrDefExt = strchr(filter.c_str(), '\0') + 1;

	//Run open save dialog and wait for output
	if (GetSaveFileName(&dialogConfig) == TRUE)
		return dialogConfig.lpstrFile; //Return selected files path

	//If no file picked return empty
	return {};
}

void ShowMessageBox(const string& text, const string& caption, u32 type)
{
	MessageBoxA(hwnd, text.c_str(), caption.c_str(), type);
}