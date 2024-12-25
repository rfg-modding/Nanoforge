using Silk.NET.Vulkan;

namespace Nanoforge.Render.Resources;

public unsafe class VkMemory
{
    protected readonly RenderContext Context;
    protected Vk Vk => Context.Vk;
    protected Device Device => Context.Device;

    protected DeviceMemory Memory;
    protected VkMemory(RenderContext context) => Context = context;

    protected bool HostMapped;
    
    public Result MapMemory(ref void* pData)
    {
        HostMapped = true;
        return Vk.MapMemory(Device, Memory, 0, Vk.WholeSize, 0, ref pData);
    }
    
    public void UnmapMemory()
    {
        HostMapped = false;
        Vk.UnmapMemory(Device, Memory);
    }
}