using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
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
    public Queue GraphicsQueue;
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
        
    };
    
    //TODO: Will have to change this to not use IWindow when porting this to Avalonia
    //TODO: Should some of this init code be stuck in a bootstrap class or something?
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
        var indices = FindQueueFamilies(PhysicalDevice);

        var uniqueQueueFamilies = new[] { indices.GraphicsFamily!.Value };
        uniqueQueueFamilies = uniqueQueueFamilies.Distinct().ToArray();

        using var mem = GlobalMemory.Allocate(uniqueQueueFamilies.Length * sizeof(DeviceQueueCreateInfo));
        var queueCreateInfos = (DeviceQueueCreateInfo*)Unsafe.AsPointer(ref mem.GetPinnableReference());

        float queuePriority = 1.0f;
        for (int i = 0; i < uniqueQueueFamilies.Length; i++)
        {
            queueCreateInfos[i] = new()
            {
                SType = StructureType.DeviceQueueCreateInfo,
                QueueFamilyIndex = uniqueQueueFamilies[i],
                QueueCount = 1,
                PQueuePriorities = &queuePriority
            };
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

        Vk.GetDeviceQueue(Device, indices.GraphicsFamily!.Value, 0, out GraphicsQueue);

        if (EnableValidationLayers)
        {
            SilkMarshal.Free((nint)deviceCreateInfo.PpEnabledLayerNames);
        }

        SilkMarshal.Free((nint)deviceCreateInfo.PpEnabledExtensionNames);
#endregion

#region Create main command pool
        var queueFamilyIndices = FindQueueFamilies(PhysicalDevice);

        CommandPoolCreateInfo poolInfo = new()
        {
            SType = StructureType.CommandPoolCreateInfo,
            QueueFamilyIndex = queueFamilyIndices.GraphicsFamily!.Value,
            Flags = CommandPoolCreateFlags.ResetCommandBufferBit
        };

        if (Vk.CreateCommandPool(Device, in poolInfo, null, out CommandPool) != Result.Success)
        {
            throw new Exception("failed to create command pool!");
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


        uint i = 0;
        foreach (var queueFamily in queueFamilies)
        {
            if (queueFamily.QueueFlags.HasFlag(QueueFlags.GraphicsBit))
            {
                indices.GraphicsFamily = i;
            }

            if (indices.IsComplete())
            {
                break;
            }

            i++;
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
    
    public CommandBuffer BeginSingleTimeCommands()
    {
        CommandBufferAllocateInfo allocateInfo = new()
        {
            SType = StructureType.CommandBufferAllocateInfo,
            Level = CommandBufferLevel.Primary,
            CommandPool = CommandPool,
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

    public void EndSingleTimeCommands(CommandBuffer commandBuffer)
    {
        Vk.EndCommandBuffer(commandBuffer);

        SubmitInfo submitInfo = new()
        {
            SType = StructureType.SubmitInfo,
            CommandBufferCount = 1,
            PCommandBuffers = &commandBuffer,
        };

        Vk.QueueSubmit(GraphicsQueue, 1, in submitInfo, default);
        Vk.QueueWaitIdle(GraphicsQueue);

        Vk.FreeCommandBuffers(Device, CommandPool, 1, in commandBuffer);
    }
    
    private void CreateStagingBuffer()
    {
        StagingBuffer = new VkBuffer(this, StagingBufferSize, BufferUsageFlags.TransferSrcBit, MemoryPropertyFlags.HostVisibleBit | MemoryPropertyFlags.HostCoherentBit, canGrow: true);
    }
    
    public CommandBuffer AllocateCommandBuffer()
    {
        var allocInfo = new CommandBufferAllocateInfo
        {
            SType = StructureType.CommandBufferAllocateInfo,
            CommandPool = CommandPool,
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