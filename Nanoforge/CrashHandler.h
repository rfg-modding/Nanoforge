#pragma once
#include <ext/WindowsWrapper.h>
#include "common/String.h"

string ExceptionCodeToString(DWORD code);
int GenerateMinidump(EXCEPTION_POINTERS* pExceptionPointers);
LONG WINAPI CrashHandler(EXCEPTION_POINTERS* exceptions);
void SetupCrashHandler();