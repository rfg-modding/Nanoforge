#include "application/Application.h"
#include <ext/WindowsWrapper.h>
#include "util/Profiler.h"
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

int WINAPI main(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    try
    {
        Application app(hInstance);
        app.Run();
    }
    catch(std::exception ex)
    {
        std::cout << "Exception caught in WinMain! Message: " << ex.what() << "\n";
    }

    return 0;
}