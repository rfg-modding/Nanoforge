#pragma once
#include "common/Typedefs.h"

//Constexpr capable FNV-1A string hash
//On the current version of MSVC this only runs at compile time if you explicitly declare a constexpr variable.
//E.g. SomeFunc("MyString"_h) is done at runtime.
//See this site for implementation details on FNV-1A: http://www.isthe.com/chongo/tech/comp/fnv/index.html#FNV-1
constexpr u64 FNV_1A(std::string_view str)
{
    u64 result = 14695981039346656037ULL; //FNV offset basis
    for (char c : str)
    {
        result ^= c;
        result *= 1099511628211ULL; //FNV prime
    }

    return result;
}

//Comptime hash. Use by putting H after a string literal. E.g. u64 hash = "myString"_hash
constexpr u64 operator"" _h(const char* str, size_t size)
{
    return FNV_1A({ str, size });
}