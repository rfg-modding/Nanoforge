using System;

namespace Win32
{
	extension Win32
	{
        [Import("kernel32.lib"), CLink, CallingConvention(.Stdcall)]
        public static extern BOOL QueryPerformanceCounter(out LARGE_INTEGER lpPerformanceCount);
        [Import("kernel32.lib"), CLink, CallingConvention(.Stdcall)]
        public static extern BOOL QueryPerformanceFrequency(out LARGE_INTEGER lpFrequency);
	}
}