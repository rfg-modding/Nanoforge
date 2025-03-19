using System.Runtime.InteropServices;

namespace Nanoforge.Render.Misc;

[StructLayout(LayoutKind.Explicit)]
public struct MaterialInstance
{
    [field: FieldOffset(0)]
    public int Texture0;
    [FieldOffset(4)]
    public int Texture1;
    [FieldOffset(8)]
    public int Texture2;
    [FieldOffset(12)]
    public int Texture3;
    [FieldOffset(16)]
    public int Texture4;
    [FieldOffset(20)]
    public int Texture5;
    [FieldOffset(24)]
    public int Texture6;
    [FieldOffset(28)]
    public int Texture7;
    [FieldOffset(32)]
    public int Texture8;
    [FieldOffset(36)]
    public int Texture9;
    
    [FieldOffset(40)]
    private int _padding0;
    [FieldOffset(44)]
    private int _padding1;
}