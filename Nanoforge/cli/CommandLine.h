#pragma once
#include "Common/Typedefs.h"
#include <ext/WindowsWrapper.h>

//Get argument count from WinMain lpCmdLine argument
int GetArgc(LPSTR lpCmdLine);
//Run CLI version of nanoforge instead of the GUI
void ProcessCommandLine(LPSTR lpCmdLine);