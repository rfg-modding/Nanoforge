#include "application/Application.h"
#include <ext/WindowsWrapper.h>
#include <iostream>
#include <exception>

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