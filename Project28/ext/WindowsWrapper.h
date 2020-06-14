#pragma once

//Todo: Need to pack win10 sdk DLLs with app for win7 users
//Target windows 7 with win10 sdk
#include <winsdkver.h>
#define _WIN32_WINNT 0x0601
#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>