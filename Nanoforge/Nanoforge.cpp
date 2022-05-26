#include "application/Application.h"
#include <ext/WindowsWrapper.h>
#include "gui/util/WinUtil.h"
#include "cli/CommandLine.h"
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
    //Catches unhandled SEH exceptions. Attempts to create minidump file.
    SetupCrashHandler();

    if (GetArgc(lpCmdLine) == 1) //Run normal gui app if no args passed
    {
        Application app(hInstance);
        app.Run();
    }
    else //If > 1 args passed, run CLI
    {
        ProcessCommandLine(lpCmdLine);
    }

    return 0;
}
#pragma warning(default:4100)