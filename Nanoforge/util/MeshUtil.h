#pragma once
#include "common/Typedefs.h"
#include <span>

//Converts any type of std::span to a std::span<u8>
template<class T>
std::span<u8> ToByteSpan(std::span<T> in)
{
    return std::span<u8>((u8*)in.data(), in.size_bytes());
}