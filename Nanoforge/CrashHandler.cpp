#include "CrashHandler.h"
#include <dbghelp.h>
#include <shellapi.h>
#include <shlobj.h>
#include "Log.h"
#include "gui/util/WinUtil.h"
#include "BuildConfig.h"
#include <strsafe.h>
#include <iomanip>
#include <ctime>
#include <sstream>
#include <spdlog/fmt/fmt.h>

string GetDateTime()
{
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%d-%m-%Y %H-%M-%S");
    return oss.str();
}

int GenerateMinidump(EXCEPTION_POINTERS* pExceptionPointers)
{
    HANDLE hDumpFile;
    MINIDUMP_EXCEPTION_INFORMATION ExpParam;
    string outPath = fmt::format("./CrashDump_Nanoforge_{}.dmp", GetDateTime());

    //Setup minidump
    ExpParam.ThreadId = GetCurrentThreadId();
    ExpParam.ExceptionPointers = pExceptionPointers;
    ExpParam.ClientPointers = TRUE;

    MINIDUMP_TYPE dumpType = (MINIDUMP_TYPE)(MiniDumpWithPrivateReadWriteMemory | MiniDumpIgnoreInaccessibleMemory |
        MiniDumpWithDataSegs | MiniDumpWithHandleData | MiniDumpWithFullMemoryInfo |
        MiniDumpWithThreadInfo | MiniDumpWithUnloadedModules);

    //Write dmp file
    hDumpFile = CreateFile(outPath.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);
    bool success = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hDumpFile, dumpType, &ExpParam, NULL, NULL);
    return EXCEPTION_EXECUTE_HANDLER;
}

//Override windows crash handler to catch unhandled SEH exceptions
LONG WINAPI CrashHandler(EXCEPTION_POINTERS* exceptions)
{
    //Get crash data
    PEXCEPTION_RECORD& record = exceptions->ExceptionRecord;
    bool continuable = record->ExceptionFlags == 0;
    string errorCodeString = ExceptionCodeToString(record->ExceptionCode);
    string errorMessage = fmt::format("A fatal error has occurred! Nanoforge will crash once you press \"OK\". Please wait for the window to close so the crash dump can save. After that send MasterLog.log and the most recent CrashDump file to moneyl on the RF discord. If you're not on the RF discord you can join it by making a discord account and going to RFChat.com. Exception code: {}. Continuable: {}", errorCodeString, continuable);

    //Log crash data, generate minidump, and notify user via message box
    GenerateMinidump(exceptions);
    ShowMessageBox(errorMessage, "Unhandled exception encountered!", MB_OK);
    LOG_ERROR(errorMessage);
    return EXCEPTION_CONTINUE_SEARCH; //Crash regardless of continuability. In testing I found it'd endlessly call the crash handler when continuing after some continuable exceptions
}

//Setup windows crash handler
void SetupCrashHandler()
{
    SetUnhandledExceptionFilter(CrashHandler);
}

string ExceptionCodeToString(DWORD code)
{
    switch (code)
    {
    case EXCEPTION_ACCESS_VIOLATION:
        return "EXCEPTION_ACCESS_VIOLATION";
    case EXCEPTION_DATATYPE_MISALIGNMENT:
        return "EXCEPTION_DATATYPE_MISALIGNMENT";
    case EXCEPTION_BREAKPOINT:
        return "EXCEPTION_BREAKPOINT";
    case EXCEPTION_SINGLE_STEP:
        return "EXCEPTION_SINGLE_STEP";
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
        return "EXCEPTION_ARRAY_BOUNDS_EXCEEDED";
    case EXCEPTION_FLT_DENORMAL_OPERAND:
        return "EXCEPTION_FLT_DENORMAL_OPERAND";
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
        return "EXCEPTION_FLT_DIVIDE_BY_ZERO";
    case EXCEPTION_FLT_INEXACT_RESULT:
        return "EXCEPTION_FLT_INEXACT_RESULT";
    case EXCEPTION_FLT_INVALID_OPERATION:
        return "EXCEPTION_FLT_INVALID_OPERATION";
    case EXCEPTION_FLT_OVERFLOW:
        return "EXCEPTION_FLT_OVERFLOW";
    case EXCEPTION_FLT_STACK_CHECK:
        return "EXCEPTION_FLT_STACK_CHECK";
    case EXCEPTION_FLT_UNDERFLOW:
        return "EXCEPTION_FLT_UNDERFLOW";
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
        return "EXCEPTION_INT_DIVIDE_BY_ZERO";
    case EXCEPTION_INT_OVERFLOW:
        return "EXCEPTION_INT_OVERFLOW";
    case EXCEPTION_PRIV_INSTRUCTION:
        return "EXCEPTION_PRIV_INSTRUCTION";
    case EXCEPTION_IN_PAGE_ERROR:
        return "EXCEPTION_IN_PAGE_ERROR";
    case EXCEPTION_ILLEGAL_INSTRUCTION:
        return "EXCEPTION_ILLEGAL_INSTRUCTION";
    case EXCEPTION_NONCONTINUABLE_EXCEPTION:
        return "EXCEPTION_NONCONTINUABLE_EXCEPTION";
    case EXCEPTION_STACK_OVERFLOW:
        return "EXCEPTION_STACK_OVERFLOW";
    case EXCEPTION_INVALID_DISPOSITION:
        return "EXCEPTION_INVALID_DISPOSITION";
    case EXCEPTION_GUARD_PAGE:
        return "EXCEPTION_GUARD_PAGE";
    case EXCEPTION_INVALID_HANDLE:
        return "EXCEPTION_INVALID_HANDLE";
    default:
        return "UNKNOWN_EXCEPTION_CODE";
    }
}