#pragma once
#include "common/Typedefs.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/details/null_mutex.h>
#include <spdlog/sinks/base_sink.h>
#include <exception>
#include <mutex>

extern Handle<spdlog::logger> Log;

//Throw an exception and log it's error message + info about the error location
#define THROW_EXCEPTION(formatString, ...) \
{ \
    string error = fmt::format(formatString, ##__VA_ARGS__); \
    string errorString = fmt::format("Exception thrown in {}(). {}:{}. Error: \"{}\"", __FUNCTION__, __FILE__, __LINE__, error); \
    Log->error(errorString); \
    throw std::runtime_error(errorString); \
} \

//Log each time the function the macro is placed in is called. Mainly used in functions that are called infrequently to help pinpoint where an error occurred.
//E.g. Most init functions have this so if a crash occurs on startup you can see how far into the init process it got by looking at the logs.
#define TRACE() \
Log->info("Called {}(). {}:{}.", __FUNCTION__, __FILE__, __LINE__); \

