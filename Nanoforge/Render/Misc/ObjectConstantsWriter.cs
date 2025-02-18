using System;

namespace Nanoforge.Render.Misc;

public class ObjectConstantsWriter
{
    public readonly int MaxObjects;
    public uint NumObjects { get; private set; } = 0;
    public PerObjectConstants[] Constants;
    
    public ObjectConstantsWriter(int maxObjects)
    {
        MaxObjects = maxObjects;
        Constants = new PerObjectConstants[maxObjects];
    }

    public void Reset()
    {
        NumObjects = 0;
    }

    public void AddConstant(PerObjectConstants constant)
    {
        if (NumObjects == MaxObjects)
            throw new Exception($"Exceeded maximum render object count of {MaxObjects}. Please recompile Nanoforge with a higher maximum or rewrite the code to grow the buffer on demand.");
        
        Constants[NumObjects++] = constant;
    }
}