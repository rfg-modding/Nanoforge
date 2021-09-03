#pragma once
#include "common/Typedefs.h"
#include "Log.h"
#include <span>

//Converts any type of std::span to a std::span<u8>
template<class T>
std::span<u8> ToByteSpan(std::span<T> in)
{
    return std::span<u8>((u8*)in.data(), in.size_bytes());
}

//Convert std::span<From> to std::span<To>. Throws an exception if input span isn't cleanly divisible by sizeof(To).
template<class From, class To>
std::span<To> ConvertSpan(std::span<From> in)
{
    if (in.size_bytes() % sizeof(To) != 0)
        THROW_EXCEPTION("Invalid input span passed to ConvertSpan<To, From>(std::span<From> in). in.size_bytes() must be cleanly divisible by sizeof(To).");

    return std::span<To>((To*)in.data(), in.size_bytes() / sizeof(To));
}