#include "WinUtil.h"
#include "common/string/String.h"
#include "BuildConfig.h"
#include "Log.h"
#include <ext/WindowsWrapper.h>
#include <wintoastlib.h>
#include <filesystem>
#pragma warning(disable:4996)
#pragma warning(disable:26812) //Disable library warnings
#include <nfd.h>
#pragma warning(default:26812)
#pragma warning(default:4996)

using namespace WinToastLib;

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

int ShowMessageBox(const string& text, const string& caption, u32 type, u32 icon)
{
	return MessageBoxA(hwnd, text.c_str(), caption.c_str(), type | icon);
}

//WinToast requires a handler. Currently does nothing since no special behavior is needed.
class BasicToastHandler : public IWinToastHandler
{
public:
	BasicToastHandler() { }
	void toastActivated() const override { }
	void toastActivated(int actionIndex) const override { }
	void toastDismissed(WinToastDismissalReason state) const override { }
	void toastFailed() const override { }
};

static bool WinToastInitialized = false;
static bool WinToastInitError = false;
static BasicToastHandler ToastHandler;
void ToastNotification(const string& text, const string& title)
{
	if (WinToastInitError)
		return;
	if (!WinToast::isCompatible())
	{
		Log->warn("This system doesn't support toast notifications.");
		WinToastInitError = true;
	}
	if (!WinToastInitialized)
	{
		WinToast* winToast = WinToast::instance();
		winToast->setAppName(L"Nanoforge");
		const std::wstring aumi = WinToast::configureAUMI(L"", L"Nanoforge", L"", String::ToWideString(BuildConfig::Version));
		winToast->setAppUserModelId(aumi);
		WinToast::WinToastError error;
		if (!winToast->initialize(&error))
		{
			Log->error("Failed to initialize WinToast! Error code: {}", (size_t)error);
			WinToastInitError = true;
			return;
		}

		WinToastInitialized = true;
	}

	WinToast* winToast = WinToast::instance();
	if (title != "") //Bold title + text
	{
		auto toast = WinToastTemplate(WinToastTemplate::Text02);
		toast.setFirstLine(String::ToWideString(title));
		toast.setSecondLine(String::ToWideString(text));
		WinToast::WinToastError error;
		if (!winToast->showToast(toast, &ToastHandler, &error))
		{
			Log->error("Failed to show toast notification. Error code: {}", (size_t)error);
			return;
		}
	}
	else //Just text
	{
		auto toast = WinToastTemplate(WinToastTemplate::Text01);
		toast.setFirstLine(String::ToWideString(title));
		WinToast::WinToastError error;
		if (!winToast->showToast(toast, &ToastHandler, &error))
		{
			Log->error("Failed to show toast notification. Error code: {}", (size_t)error);
			return;
		}
	}
}