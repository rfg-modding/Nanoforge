using System;

namespace Win32
{
    public enum TRACKMOUSEEVENT_FLAGS : uint32
    {
    	CANCEL = 2147483648,
    	HOVER = 1,
    	LEAVE = 2,
    	NONCLIENT = 16,
    	QUERY = 1073741824,
    }

    [CRepr]
    public struct TRACKMOUSEEVENT
    {
    	public uint32 cbSize;
    	public TRACKMOUSEEVENT_FLAGS dwFlags;
    	public HWND hwndTrack;
    	public uint32 dwHoverTime;
    }

	extension Win32
	{
        [Import("user32.lib"), CLink, CallingConvention(.Stdcall)]
        public static extern HWND GetCapture();
        [Import("user32.lib"), CLink, CallingConvention(.Stdcall)]
        public static extern HWND SetCapture(HWND hWnd);
        [Import("user32.lib"), CLink, CallingConvention(.Stdcall)]
        public static extern BOOL ReleaseCapture();
        [Import("user32.lib"), CLink, CallingConvention(.Stdcall)]
        public static extern BOOL TrackMouseEvent(TRACKMOUSEEVENT* lpEventTrack);
        [Import("user32.lib"), CLink, CallingConvention(.Stdcall)]
        public static extern int16 GetKeyState(int32 nVirtKey);
        [Import("user32.lib"), CLink, CallingConvention(.Stdcall)]
        public static extern int16 GetAsyncKeyState(int32 vKey);
	}
}