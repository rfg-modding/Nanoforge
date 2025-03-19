using System;

namespace Nanoforge.Render.Misc;

public class GpuFrameDataWriter
{
    public readonly int MaxObjects;
    public readonly int MaxMaterials;
    public uint NumObjects { get; private set; } = 0;
    public int NumMaterials { get; private set; } = 0;
    public PerObjectConstants[] Constants;
    public MaterialInstance[] Materials;
    
    public GpuFrameDataWriter(int maxObjects, int maxMaterials)
    {
        MaxObjects = maxObjects;
        MaxMaterials = maxMaterials;
        Constants = new PerObjectConstants[maxObjects];
        Materials = new MaterialInstance[maxMaterials];
    }

    public void Reset()
    {
        NumObjects = 0;
        NumMaterials = 0;
    }

    public uint AddObject(PerObjectConstants constant)
    {
        if (NumObjects == MaxObjects)
            throw new Exception($"Exceeded maximum render object count of {MaxObjects}. Please recompile Nanoforge with a higher maximum or rewrite the code to grow the buffer on demand.");

        uint objectIndex = NumObjects;
        Constants[NumObjects++] = constant;
        return objectIndex;
    }

    public int AddMaterialInstance(MaterialInstance materialInstance)
    {
        if (NumMaterials == MaxMaterials)
            throw new Exception($"Exceeded maximum material instance count of {MaxMaterials}. Please recompile Nanoforge with a higher maximum or rewrite the code to grow the buffer on demand.");
        
        int materialIndex = NumMaterials;
        Materials[NumMaterials++] = materialInstance;
        return materialIndex;
    }
}