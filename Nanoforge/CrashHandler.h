#pragma once
#include <ext/WindowsWrapper.h>
#include "Log.h"

string ExceptionCodeToString(DWORD code);

//Override windows crash handler to catch unhandled SEH exceptions
//Currently this doesn't log much info about the crash, but at least logs the reason for the crash and displays an error box instead of closing the window without warning.
LONG WINAPI CrashHandler(EXCEPTION_POINTERS* exceptions)
{
    //Get crash data
    PEXCEPTION_RECORD& record = exceptions->ExceptionRecord;
    bool continuable = record->ExceptionFlags == 0;
    string errorCodeString = ExceptionCodeToString(record->ExceptionCode);
    string errorMessage = fmt::format("A fatal error has occurred! Nanoforge will crash once you press \"OK\". After that send MasterLog.log to moneyl on the RF discord. If you're not on the RF discord you can join it by making a discord account and going to RFChat.com. Exception code: {}. Continuable: {}", errorCodeString, continuable);

    //Log crash data and notify user via message box
    LOG_ERROR(errorMessage);
    ShowMessageBox(errorMessage, "Unhandled exception encountered!", MB_OK);
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