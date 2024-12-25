using Silk.NET.Vulkan;

namespace Nanoforge.Render.Misc;

internal struct SwapChainSupportDetails
{
    public SurfaceCapabilitiesKHR Capabilities;
    public SurfaceFormatKHR[] Formats;
    public PresentModeKHR[] PresentModes;
}