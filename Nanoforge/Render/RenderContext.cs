using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Threading;
using Nanoforge.Render.Misc;
using Nanoforge.Render.Resources;
using Serilog;
using Silk.NET.Core;
using Silk.NET.Core.Native;
using Silk.NET.Vulkan;
using Silk.NET.Vulkan.Extensions.EXT;

namespace Nanoforge.Render;

public unsafe class RenderContext : IDisposable
{
    public readonly Vk Vk;

    public Device Device;
    public PhysicalDevice PhysicalDevice;

    public Instance Instance;
    public DebugUtilsMessengerEXT DebugUtilsMessenger;
    public CommandPool CommandPool;
    public CommandPool TransferCommandPool;
    public Queue GraphicsQueue;
    public Queue TransferQueue;
    private ExtDebugUtils? _debugUtils;

    public VkBuffer StagingBuffer = null!;
    public const int StagingBufferSize = 60000000;
    
#if DEBUG
    public const bool EnableValidationLayers = true;
#else
    public const bool EnableValidationLayers = false;
#endif

    public RenderPass PrimaryRenderPass;
    
    private readonly string[] _validationLayers = new[]
    {
        "VK_LAYER_KHRONOS_validation"
    };

    private readonly string[] _deviceExtensions = new string[]
    {
        "VK_KHR_shader_draw_parameters"
    };
    
    public RenderContext()
    {
#region Create Instance
        Vk = Vk.GetApi();

        if (EnableValidationLayers && !CheckValidationLayerSupport())
        {
            throw new Exception("Validation layers requested, but not available!");
        }
        
        ApplicationInfo appInfo = new()
        {
            SType = StructureType.ApplicationInfo,
            PApplicationName = (byte*)Marshal.StringToHGlobalAnsi("CSVulkanRenderer"),
            ApplicationVersion = new Version32(1, 0, 0),
            PEngineName = (byte*)Marshal.StringToHGlobalAnsi("No Engine"),
            EngineVersion = new Version32(1, 0, 0),
            ApiVersion = Vk.Version12
        };

        InstanceCreateInfo createInfo = new()
        {
            SType = StructureType.InstanceCreateInfo,
            PApplicationInfo = &appInfo
        };

        var extensions = GetRequiredExtensions();
        createInfo.EnabledExtensionCount = (uint)extensions.Length;
        createInfo.PpEnabledExtensionNames = (byte**)SilkMarshal.StringArrayToPtr(extensions); ;
        
        if (EnableValidationLayers)
        {
            createInfo.EnabledLayerCount = (uint)_validationLayers.Length;
            createInfo.PpEnabledLayerNames = (byte**)SilkMarshal.StringArrayToPtr(_validationLayers);

            DebugUtilsMessengerCreateInfoEXT debugCreateInfo = new();
            PopulateDebugMessengerCreateInfo(ref debugCreateInfo);
            createInfo.PNext = &debugCreateInfo;
        }
        else
        {
            createInfo.EnabledLayerCount = 0;
            createInfo.PNext = null;
        }

        if (Vk.CreateInstance(in createInfo, null, out Instance) != Result.Success)
        {
            throw new Exception("Failed to create instance!");
        }

        Marshal.FreeHGlobal((IntPtr)appInfo.PApplicationName);
        Marshal.FreeHGlobal((IntPtr)appInfo.PEngineName);
        SilkMarshal.Free((nint)createInfo.PpEnabledExtensionNames);

        if (EnableValidationLayers)
        {
            SilkMarshal.Free((nint)createInfo.PpEnabledLayerNames);
        }
#endregion
        
#region Setup debug messenger
        if (EnableValidationLayers)
        {
            //TryGetInstanceExtension equivilant to method CreateDebugUtilsMessengerEXT from original tutorial.
            if (!Vk.TryGetInstanceExtension(Instance, out _debugUtils))
                return;

            DebugUtilsMessengerCreateInfoEXT debugUtilsCreateInfo = new();
            PopulateDebugMessengerCreateInfo(ref debugUtilsCreateInfo);

            if (_debugUtils!.CreateDebugUtilsMessenger(Instance, in debugUtilsCreateInfo, null, out DebugUtilsMessenger) != Result.Success)
            {
                throw new Exception("failed to set up debug messenger!");
            }
        }
#endregion
        
#region Pick physical device
        var devices = Vk.GetPhysicalDevices(Instance);
        foreach (var device in devices)
        {
            if (IsDeviceSuitable(device))
            {
                PhysicalDevice = device;
                break;
            }
        }

        if (PhysicalDevice.Handle == 0)
        {
            throw new Exception("Failed to find a suitable GPU!");
        }
        
        Vk.GetPhysicalDeviceProperties(PhysicalDevice, out var physicalDeviceProperties);
        string physicalDeviceName = Marshal.PtrToStringAnsi((IntPtr)physicalDeviceProperties.DeviceName) ?? "Unknown device";
        Log.Information($"Renderer selected vulkan physical device: {physicalDeviceName}");
#endregion
        
#region Create logical device
        QueueFamilyIndices queueFamilyIndices = FindQueueFamilies(PhysicalDevice);
        Debug.Assert(queueFamilyIndices.GraphicsFamily != null, "queueFamilyIndices.GraphicsFamily != null");
        Debug.Assert(queueFamilyIndices.TransferFamily != null, "queueFamilyIndices.TransferFamily != null");
        Log.Information($"Selected vulkan queues. Graphics queue: {queueFamilyIndices.GraphicsFamily.Value}, Transfer queue: {queueFamilyIndices.TransferFamily.Value}");

        uint[] uniqueQueueFamilies = [queueFamilyIndices.GraphicsFamily!.Value, queueFamilyIndices.TransferFamily!.Value];
        uniqueQueueFamilies = uniqueQueueFamilies.Distinct().ToArray();

        using var mem = GlobalMemory.Allocate(uniqueQueueFamilies.Length * sizeof(DeviceQueueCreateInfo));
        DeviceQueueCreateInfo* queueCreateInfos = (DeviceQueueCreateInfo*)Unsafe.AsPointer(ref mem.GetPinnableReference());

        float* queuePriority = (float*)SilkMarshal.Allocate(sizeof(float));
        *queuePriority = 1.0f;
        uint graphicsQueueIndex;
        uint trafficsQueueIndex;
        if (uniqueQueueFamilies.Length == 1)
        {
            //The graphics and transfer queue are in the same family, so we want two of this queue type with each queue having a different index.
            graphicsQueueIndex = 0;
            trafficsQueueIndex = 1;
            queueCreateInfos[0] = new()
            {
                SType = StructureType.DeviceQueueCreateInfo,
                QueueFamilyIndex = uniqueQueueFamilies[0],
                QueueCount = 2,
                PQueuePriorities = queuePriority,
            };
        }
        else if (uniqueQueueFamilies.Length >= 2)
        {
            //The graphics and transfer queue are in different families. So we want 1 of each with each being at index 0.
            graphicsQueueIndex = 0;
            trafficsQueueIndex = 0;
            for (int i = 0; i < uniqueQueueFamilies.Length; i++)
            {
                queueCreateInfos[i] = new()
                {
                    SType = StructureType.DeviceQueueCreateInfo,
                    QueueFamilyIndex = uniqueQueueFamilies[i],
                    QueueCount = 1,
                    PQueuePriorities = queuePriority,
                };
            }
        }
        else
        {
            throw new Exception($"Unexpected unique vulkan queue family count. Expected either 1 or 2. Found {uniqueQueueFamilies.Length}.");
        }

        PhysicalDeviceFeatures deviceFeatures = new()
        {
            SamplerAnisotropy = true,
        };

        DeviceCreateInfo deviceCreateInfo = new()
        {
            SType = StructureType.DeviceCreateInfo,
            QueueCreateInfoCount = (uint)uniqueQueueFamilies.Length,
            PQueueCreateInfos = queueCreateInfos,

            PEnabledFeatures = &deviceFeatures,

            EnabledExtensionCount = (uint)_deviceExtensions.Length,
            PpEnabledExtensionNames = (byte**)SilkMarshal.StringArrayToPtr(_deviceExtensions)
        };

        if (EnableValidationLayers)
        {
            deviceCreateInfo.EnabledLayerCount = (uint)_validationLayers.Length;
            deviceCreateInfo.PpEnabledLayerNames = (byte**)SilkMarshal.StringArrayToPtr(_validationLayers);
        }
        else
        {
            deviceCreateInfo.EnabledLayerCount = 0;
        }

        if (Vk.CreateDevice(PhysicalDevice, in deviceCreateInfo, null, out Device) != Result.Success)
        {
            throw new Exception("failed to create logical device!");
        }

        SilkMarshal.Free((IntPtr)queuePriority);

        Vk.GetDeviceQueue(Device, queueFamilyIndices.GraphicsFamily!.Value, graphicsQueueIndex, out GraphicsQueue);
        Vk.GetDeviceQueue(Device, queueFamilyIndices.TransferFamily!.Value, trafficsQueueIndex, out TransferQueue);

        if (EnableValidationLayers)
        {
            SilkMarshal.Free((nint)deviceCreateInfo.PpEnabledLayerNames);
        }

        SilkMarshal.Free((nint)deviceCreateInfo.PpEnabledExtensionNames);
#endregion

#region Create main command pool

        CommandPoolCreateInfo poolInfo = new()
        {
            SType = StructureType.CommandPoolCreateInfo,
            QueueFamilyIndex = queueFamilyIndices.GraphicsFamily!.Value,
            Flags = CommandPoolCreateFlags.ResetCommandBufferBit
        };

        if (Vk.CreateCommandPool(Device, in poolInfo, null, out CommandPool) != Result.Success)
        {
            throw new Exception("Failed to create primary command pool!");
        }
#endregion

#region Create command pool for background thread data transfers
        CommandPoolCreateInfo transferPoolInfo = new()
        {
            SType = StructureType.CommandPoolCreateInfo,
            QueueFamilyIndex = queueFamilyIndices.TransferFamily!.Value,
            Flags = CommandPoolCreateFlags.ResetCommandBufferBit
        };

        if (Vk.CreateCommandPool(Device, in transferPoolInfo, null, out TransferCommandPool) != Result.Success)
        {
            throw new Exception("Failed to create transfer command pool!");
        }
#endregion

#region Create main staging buffer
        CreateStagingBuffer();
#endregion
    }

    private bool IsDeviceSuitable(PhysicalDevice device)
    {
        var indices = FindQueueFamilies(device);

        bool extensionsSupported = CheckDeviceExtensionsSupport(device);

        Vk.GetPhysicalDeviceFeatures(device, out PhysicalDeviceFeatures supportedFeatures);

        return indices.IsComplete() && extensionsSupported && supportedFeatures.SamplerAnisotropy;
    }

    private bool CheckDeviceExtensionsSupport(PhysicalDevice device)
    {
        uint extensionsCount = 0;
        Vk.EnumerateDeviceExtensionProperties(device, (byte*)null, ref extensionsCount, null);

        var availableExtensions = new ExtensionProperties[extensionsCount];
        fixed (ExtensionProperties* availableExtensionsPtr = availableExtensions)
        {
            Vk.EnumerateDeviceExtensionProperties(device, (byte*)null, ref extensionsCount, availableExtensionsPtr);
        }

        var availableExtensionNames = availableExtensions.Select(extension => Marshal.PtrToStringAnsi((IntPtr)extension.ExtensionName)).ToHashSet();

        return _deviceExtensions.All(availableExtensionNames.Contains);
    }
    
    
    private QueueFamilyIndices FindQueueFamilies(PhysicalDevice device)
    {
        var indices = new QueueFamilyIndices();

        uint queueFamilityCount = 0;
        Vk.GetPhysicalDeviceQueueFamilyProperties(device, ref queueFamilityCount, null);

        var queueFamilies = new QueueFamilyProperties[queueFamilityCount];
        fixed (QueueFamilyProperties* queueFamiliesPtr = queueFamilies)
        {
            Vk.GetPhysicalDeviceQueueFamilyProperties(device, ref queueFamilityCount, queueFamiliesPtr);
        }

        //First find any graphics capable queue
        for (uint i = 0; i < queueFamilies.Length; i++)
        {
            QueueFamilyProperties queueFamily = queueFamilies[i];
            if (queueFamily.QueueFlags.HasFlag(QueueFlags.GraphicsBit))
            {
                indices.GraphicsFamily = i;
                indices.GraphicsFamilyQueueCount = queueFamily.QueueCount;
                break;
            }
        }

        //TODO: Figure out how to do this. The transfer only queue on my device apparently couldn't support the pipeline barrier in the Texture2D layout transition code
        //Next try to find a queue that only has transfer and not graphics
        // for (uint i = 0; i < queueFamilies.Length; i++)
        // {
        //     QueueFamilyProperties queueFamily = queueFamilies[i];
        //     if (queueFamily.QueueFlags.HasFlag(QueueFlags.TransferBit) && !queueFamily.QueueFlags.HasFlag(QueueFlags.GraphicsBit))
        //     {
        //         indices.TransferFamily = i;
        //         indices.TransferFamilyQueueCount = queueFamily.QueueCount;
        //         break;
        //     }
        // }
        
        //If we failed to find a transfer only queue then just use whatever queue that's available for transfer
        if (!indices.TransferFamily.HasValue)
        {
            for (uint i = 0; i < queueFamilies.Length; i++)
            {
                QueueFamilyProperties queueFamily = queueFamilies[i];
                if (queueFamily.QueueFlags.HasFlag(QueueFlags.TransferBit))
                {
                    indices.TransferFamily = i;
                    indices.TransferFamilyQueueCount = queueFamily.QueueCount;
                    break;
                }
            }
        }
        
        //Make sure there's enough queues available if the graphics and transfer queue are in the same family
        if (indices is { GraphicsFamily: not null, TransferFamily: not null } && indices.GraphicsFamily.Value == indices.TransferFamily.Value)
        {
            if (indices.GraphicsFamilyQueueCount < 2)
            {
                string err = $"Graphics and transfer queue families are the same and there isn't enough queues available. Require 2, only {indices.GraphicsFamilyQueueCount} are available.";
                Log.Error(err);
                throw new Exception(err);
            }
        }
        
        if (!indices.IsComplete())
        {
            string err = "Failed to find valid vulkan queue families for graphics and transfer.";
            Log.Error(err);
            throw new Exception(err);
        }

        return indices;
    }
    
    private string[] GetRequiredExtensions()
    {
        List<string> extensions = new();

        if (EnableValidationLayers)
        {
            extensions.Add(ExtDebugUtils.ExtensionName);
        }

        return extensions.ToArray();
    }

    private bool CheckValidationLayerSupport()
    {
        uint layerCount = 0;
        Vk.EnumerateInstanceLayerProperties(ref layerCount, null);
        var availableLayers = new LayerProperties[layerCount];
        fixed (LayerProperties* availableLayersPtr = availableLayers)
        {
            Vk.EnumerateInstanceLayerProperties(ref layerCount, availableLayersPtr);
        }

        var availableLayerNames = availableLayers.Select(layer => Marshal.PtrToStringAnsi((IntPtr)layer.LayerName)).ToHashSet();

        return _validationLayers.All(availableLayerNames.Contains);
    }

    private uint DebugCallback(DebugUtilsMessageSeverityFlagsEXT messageSeverity, DebugUtilsMessageTypeFlagsEXT messageTypes, DebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
    {
        //TODO: Change to use Nanoforge logger
        Console.WriteLine($"validation layer: " + Marshal.PtrToStringAnsi((nint)pCallbackData->PMessage));

        return Vk.False;
    }
    
    private void PopulateDebugMessengerCreateInfo(ref DebugUtilsMessengerCreateInfoEXT createInfo)
    {
        createInfo.SType = StructureType.DebugUtilsMessengerCreateInfoExt;
        createInfo.MessageSeverity = DebugUtilsMessageSeverityFlagsEXT.VerboseBitExt |
                                     DebugUtilsMessageSeverityFlagsEXT.WarningBitExt |
                                     DebugUtilsMessageSeverityFlagsEXT.ErrorBitExt;
        createInfo.MessageType = DebugUtilsMessageTypeFlagsEXT.GeneralBitExt |
                                 DebugUtilsMessageTypeFlagsEXT.PerformanceBitExt |
                                 DebugUtilsMessageTypeFlagsEXT.ValidationBitExt;
        createInfo.PfnUserCallback = (DebugUtilsMessengerCallbackFunctionEXT)DebugCallback;
    }
    
    public uint FindMemoryType(uint typeFilter, MemoryPropertyFlags properties)
    {
        Vk.GetPhysicalDeviceMemoryProperties(PhysicalDevice, out PhysicalDeviceMemoryProperties memProperties);

        for (int i = 0; i < memProperties.MemoryTypeCount; i++)
        {
            if ((typeFilter & (1 << i)) != 0 && (memProperties.MemoryTypes[i].PropertyFlags & properties) == properties)
            {
                return (uint)i;
            }
        }

        throw new Exception("failed to find suitable memory type!");
    }
    
    public CommandBuffer BeginSingleTimeCommands(CommandPool pool)
    {
        CommandBufferAllocateInfo allocateInfo = new()
        {
            SType = StructureType.CommandBufferAllocateInfo,
            Level = CommandBufferLevel.Primary,
            CommandPool = pool,
            CommandBufferCount = 1,
        };

        Vk.AllocateCommandBuffers(Device, in allocateInfo, out CommandBuffer commandBuffer);

        CommandBufferBeginInfo beginInfo = new()
        {
            SType = StructureType.CommandBufferBeginInfo,
            Flags = CommandBufferUsageFlags.OneTimeSubmitBit,
        };

        Vk.BeginCommandBuffer(commandBuffer, in beginInfo);

        return commandBuffer;
    }

    public void EndSingleTimeCommands(CommandBuffer commandBuffer, CommandPool pool, Queue queue)
    {
        Vk.EndCommandBuffer(commandBuffer);

        SubmitInfo submitInfo = new()
        {
            SType = StructureType.SubmitInfo,
            CommandBufferCount = 1,
            PCommandBuffers = &commandBuffer,
        };

        Vk.QueueSubmit(queue, 1, in submitInfo, default);
        Vk.QueueWaitIdle(queue);

        Vk.FreeCommandBuffers(Device, pool, 1, in commandBuffer);
    }
    
    private void CreateStagingBuffer()
    {
        StagingBuffer = new VkBuffer(this, StagingBufferSize, BufferUsageFlags.TransferSrcBit, MemoryPropertyFlags.HostVisibleBit | MemoryPropertyFlags.HostCoherentBit, canGrow: true);
    }
    
    public CommandBuffer AllocateCommandBuffer(CommandPool pool)
    {
        var allocInfo = new CommandBufferAllocateInfo
        {
            SType = StructureType.CommandBufferAllocateInfo,
            CommandPool = pool,
            CommandBufferCount = 1,
            Level = CommandBufferLevel.Primary
        };
        Vk.AllocateCommandBuffers(Device, allocInfo, out var commandBuffer);
        return commandBuffer;
    }

    public void Dispose()
    {
        StagingBuffer.Destroy();
        Vk.DestroyCommandPool(Device, CommandPool, null);
        Vk.DestroyCommandPool(Device, TransferCommandPool, null);
        Vk.DestroyDevice(Device, null);
        _debugUtils?.DestroyDebugUtilsMessenger(Instance, DebugUtilsMessenger, null);
        Vk.DestroyInstance(Instance, null);
        Vk.Dispose();
        _debugUtils?.Dispose();
    }

    public Fence CreateFence(FenceCreateFlags flags = FenceCreateFlags.None)
    {
        var createInfo = new FenceCreateInfo
        {
            SType = StructureType.FenceCreateInfo,
            Flags = flags
        };
        Vk.CreateFence(Device, createInfo, null, out var fence);
        return fence;
    }
    
    public void BeginCommandBuffer(CommandBuffer commandBuffer)
    {
        var beginInfo = new CommandBufferBeginInfo
        {
            SType = StructureType.CommandBufferBeginInfo,
            Flags = CommandBufferUsageFlags.OneTimeSubmitBit
        };
        Vk.BeginCommandBuffer(commandBuffer, beginInfo);
    }

    public void EndCommandBuffer(CommandBuffer cmd, Fence fence = default)
    {
        Vk.EndCommandBuffer(cmd);
        var submitInfo = new SubmitInfo
        {
            SType = StructureType.SubmitInfo,
            CommandBufferCount = 1,
            PCommandBuffers = &cmd
        };
        Vk.QueueSubmit(GraphicsQueue, 1, submitInfo, fence);
    }
}