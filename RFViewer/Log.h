#pragma once
#include "common/Typedefs.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <exception>

extern std::shared_ptr<spdlog::logger> Log;

//Throw an exception and log it's error message
#define THROW_EXCEPTION(formatString, ...) \
{ \
    string message = fmt::format(formatString, ##__VA_ARGS__); \
    Log->error("Exception thrown! Message: {}", message); \
    throw std::runtime_error(message); \
} \

