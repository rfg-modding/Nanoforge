#include "CommandLine.h"
#include "common/String.h"
#include "common/string/String.h"
#include <shellapi.h>
#include <iostream>
#include <vector>

int GetArgc(LPSTR lpCmdLine)
{
    int argc = 0;
    LPWSTR cmdLine = GetCommandLineW();
    LPWSTR* argv = CommandLineToArgvW(cmdLine, &argc);
    return argc;
}

std::vector<string> GetArgs(LPSTR lpCmdLine)
{
    //Extract command line args from lpCmdLine
    int argc = 0;
    std::vector<string> out = {};
    LPWSTR cmdLine = GetCommandLineW();
    LPWSTR* argv = CommandLineToArgvW(cmdLine, &argc);
    for (size_t i = 0; i < argc; i++)
    {
        //Convert wide string to narrow
        std::wstring wide(argv[i]);
        std::string narrow(wide.begin(), wide.end());
        out.push_back(narrow);
    }
    return out;
}

void ProcessCommandLine(LPSTR lpCmdLine)
{
    std::vector<string> args = GetArgs(lpCmdLine);
    std::cout << "CLI not yet implemented.\n";
    std::cout << "Args:\n";
    for (string& arg : args)
        std::cout << "    " << arg << "\n";
}