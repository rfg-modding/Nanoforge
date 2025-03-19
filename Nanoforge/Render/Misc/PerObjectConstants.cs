using System.Numerics;
using System.Runtime.InteropServices;

namespace Nanoforge.Render.Misc;

[StructLayout(LayoutKind.Explicit)]
public struct PerObjectConstants()
{
    [FieldOffset(0)]
    public Matrix4x4 Model;
    
    [FieldOffset(64)]
    public Vector4 WorldPosition;

    //Padding needed due to shader padding rules. See here: https://registry.khronos.org/OpenGL/specs/gl/glspec45.core.pdf#page=159 Same rules apply even though we're using Vulkan.
    //In this case its due to rule 9.
    [FieldOffset(80)]
    public int MaterialIndex;
    
    [FieldOffset(84)]
    private int _padding0;
    
    [FieldOffset(88)]
    private int _padding1;
    
    [FieldOffset(92)]
    private int _padding2;
}