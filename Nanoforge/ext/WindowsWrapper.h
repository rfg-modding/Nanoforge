#pragma once

#pragma warning(disable:4005) //Disable repeat definitions for windows defines
//Target windows 7 with win10 sdk
#include <winsdkver.h>
#define _WIN32_WINNT 0x0601
#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#pragma warning(default:4005)