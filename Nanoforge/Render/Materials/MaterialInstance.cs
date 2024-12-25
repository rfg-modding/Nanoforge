using Silk.NET.Vulkan;

namespace Nanoforge.Render.Materials;

//Variant of a material. Has the same vertex input layout, primitive topology, shaders, render passes, etc. But the inputs such as the textures used and constant buffers can vary.
public class MaterialInstance
{
    public MaterialPipeline Pipeline;
    private DescriptorSet[] _descriptorSets;
    public string Name => Pipeline.Name;

    public MaterialInstance(MaterialPipeline pipeline, DescriptorSet[] descriptorSets)
    {
        Pipeline = pipeline;
        _descriptorSets = descriptorSets;
    }

    public unsafe void Bind(RenderContext context, CommandBuffer commandBuffer, uint frameIndex)
    {
        context.Vk.CmdBindDescriptorSets(commandBuffer, PipelineBindPoint.Graphics, Pipeline.Layout, 0, 1, in _descriptorSets[frameIndex], 0, null);
    }
}