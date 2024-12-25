using System;
using System.Collections.Generic;
using Silk.NET.Vulkan;

namespace Nanoforge.Render.Materials;

//Originally based on the DescriptorAllocatorGrowable from vkguide.dev: https://vkguide.dev/docs/new_chapter_4/descriptor_abstractions/
//It manages a list of descriptor pools and allocates new ones on demand when more room is needed for descriptor sets.
public class DescriptorAllocator
{
    private RenderContext _context;
    private PoolSizeRatio[] _ratios;
    private List<DescriptorPool> _fullPools = new();
    private List<DescriptorPool> _readyPools = new();
    private uint _setsPerPool;
    private const uint MaxSetsPerPool = 4096;
    
    public struct PoolSizeRatio
    {
        public DescriptorType Type;
        public float Ratio;
    }

    public DescriptorAllocator(RenderContext context, uint maxSets, PoolSizeRatio[] poolRatios)
    {
        _context = context;
        _ratios = poolRatios;
        
        DescriptorPool newPool = CreatePool(maxSets, poolRatios);
        _setsPerPool = (uint)(maxSets * 1.5f); //Grow it next allocation
        _readyPools.Add(newPool);
    }

    public void ClearPools()
    {
        foreach (DescriptorPool pool in _readyPools)
        {
            _context.Vk.ResetDescriptorPool(_context.Device, pool, 0);
        }
        foreach (DescriptorPool pool in _fullPools)
        {
            _context.Vk.ResetDescriptorPool(_context.Device, pool, 0);
            _readyPools.Add(pool);
        }
        _fullPools.Clear();
    }

    public unsafe void Destroy()
    {
        foreach (DescriptorPool pool in _readyPools)
        {
            _context.Vk.DestroyDescriptorPool(_context.Device, pool, null);
        }
        foreach (DescriptorPool pool in _fullPools)
        {
            _context.Vk.DestroyDescriptorPool(_context.Device, pool, null);
        }
        _readyPools.Clear();
        _fullPools.Clear();
    }

    public unsafe DescriptorSet Allocate(DescriptorSetLayout layout, void* next = null)
    {
        DescriptorPool poolToUse = GetPool();

        DescriptorSetAllocateInfo allocInfo = new()
        {
            SType = StructureType.DescriptorSetAllocateInfo,
            PNext = next,
            DescriptorPool = poolToUse,
            DescriptorSetCount = 1,
            PSetLayouts = &layout
        };

        DescriptorSet descriptorSet;
        Result result = _context.Vk.AllocateDescriptorSets(_context.Device, &allocInfo, out descriptorSet);
        
        if (result == Result.ErrorOutOfPoolMemory || result == Result.ErrorFragmentedPool)
        {
            //Allocation failed. Try again with new pool.
            _fullPools.Add(poolToUse);

            poolToUse = GetPool();
            allocInfo.DescriptorPool = poolToUse;

            result = _context.Vk.AllocateDescriptorSets(_context.Device, &allocInfo, out descriptorSet);
        }
        if (result != Result.Success)
        {
            throw new Exception($"DescriptorAllocator failed to allocate descriptor set. Result: {result}");
        }
        
        _readyPools.Add(poolToUse);
        return descriptorSet;
    }

    private DescriptorPool GetPool()
    {
        DescriptorPool newPool;
        if (_readyPools.Count != 0)
        {
            int lastIndex = _readyPools.Count - 1;
            newPool = _readyPools[lastIndex];
            _readyPools.RemoveAt(lastIndex);
        }
        else
        {
            //Need to create a new pool
            newPool = CreatePool(_setsPerPool, _ratios);
            
            //Increase sets per pool so that each successive allocation is larger
            _setsPerPool = (uint)(_setsPerPool * 1.5f);
            if (_setsPerPool > MaxSetsPerPool)
            {
                _setsPerPool = MaxSetsPerPool;
            }
        }

        return newPool;
    }

    private unsafe DescriptorPool CreatePool(uint setCount, Span<PoolSizeRatio> poolRatios)
    {
        var poolSizes = new DescriptorPoolSize[poolRatios.Length];
        for (int i = 0; i < poolRatios.Length; i++)
        {
            PoolSizeRatio ratio = poolRatios[i];
            poolSizes[i] = new DescriptorPoolSize
            {
                Type = ratio.Type,
                DescriptorCount = (uint)(ratio.Ratio * setCount),
            };
        }

        fixed (DescriptorPoolSize* poolSizesPtr = poolSizes)
        {
            DescriptorPoolCreateInfo createInfo = new()
            {
                SType = StructureType.DescriptorPoolCreateInfo,
                Flags = 0,
                MaxSets = setCount,
                PoolSizeCount = (uint)poolSizes.Length,
                PPoolSizes = poolSizesPtr,
            };

            DescriptorPool newPool;
            if (_context.Vk.CreateDescriptorPool(_context.Device, &createInfo, null, &newPool) != Result.Success)
            {
                throw new Exception("Failed to create descriptor pool");
            }
            
            return newPool;
        }
    }
}