#include "application/Application.h"
#include <ext/WindowsWrapper.h>
#include "gui/util/WinUtil.h"
#include "util/Profiler.h"
#include "CrashHandler.h"
#include <iostream>
#include <exception>

//Memory profiling
#ifdef TRACY_ENABLE
void* operator new(std::size_t count)
{
    void* ptr = malloc(count);
    TracyAlloc(ptr, count);
    return ptr;
}

void operator delete(void* ptr) noexcept
{
    TracyFree(ptr);
    free(ptr);
}
#endif

#pragma warning(disable:4100) //Ignore unused arguments warning
int WINAPI main(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    //Catches unhandled SEH exceptions
    SetupCrashHandler();

    //Catches unhandled c++ exceptions
    try
    {
        Application app(hInstance);
        app.Run();
    }
    catch(std::exception ex)
    {
        string errorMessage = fmt::format("A fatal error has occurred! Nanoforge will crash once you press \"OK\". After that send MasterLog.log to moneyl on the RF discord. If you're not on the RF discord you can join it by making a discord account and going to RFChat.com. Error message: \"{}\"", ex.what());
        LOG_ERROR(errorMessage);
        ShowMessageBox(errorMessage, "Fatal error encountered!", MB_OK);
    }

    return 0;
}
#pragma warning(default:4100)