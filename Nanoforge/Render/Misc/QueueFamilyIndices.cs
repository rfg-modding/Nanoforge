namespace Nanoforge.Render.Misc;

internal struct QueueFamilyIndices
{
    public uint? GraphicsFamily { get; set; }
    public uint? GraphicsFamilyQueueCount { get; set; }
    public uint? TransferFamily { get; set; }
    public uint? TransferFamilyQueueCount { get; set; }

    public bool IsComplete()
    {
        return GraphicsFamily.HasValue && TransferFamily.HasValue;
    }
}