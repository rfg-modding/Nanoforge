using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Text;
using Serilog;
using Silk.NET.Core.Native;
using Silk.NET.Shaderc;
using Silk.NET.Vulkan;

namespace Nanoforge.Render.Resources;

public class Shader
{
    private RenderContext _context;
    private readonly string _shaderFilePath;
    private bool _optimize;
    public string EntryPoint { get; private set; }
    public string Name => Path.GetFileName(_shaderFilePath);
    public ShaderKind Kind { get; }

    public ShaderModule Module { get; private set; }

    public const int MaxIncludeDepth = 10;

    private List<(string path, DateTime writeTime)> _fileWriteTimes = new();
    
    public bool SourceFilesEdited
    {
        get
        {
            return _fileWriteTimes.Any(tuple => tuple.writeTime != File.GetLastWriteTime(tuple.path));
        }
    }

    public Shader(RenderContext context, string shaderFilePath, ShaderKind shaderKind, bool optimize = true, string entryPoint = "main")
    {
        _context = context;
        _shaderFilePath = shaderFilePath;
        Kind = shaderKind;
        _optimize = optimize;
        EntryPoint = entryPoint;

        LoadShader();
    }

    private unsafe void LoadShader()
    {
        if (Module.Handle != 0)
        {
            _context.Vk.DestroyShaderModule(_context.Device, Module, null);
        }

        Module = LoadAndCompileShaderFile(_shaderFilePath, EntryPoint, Kind, _optimize);
    }

    private unsafe ShaderModule CreateShaderModule(byte[] code)
    {
        ShaderModuleCreateInfo createInfo = new()
        {
            SType = StructureType.ShaderModuleCreateInfo,
            CodeSize = (nuint)code.Length,
        };

        ShaderModule shaderModule;

        fixed (byte* codePtr = code)
        {
            createInfo.PCode = (uint*)codePtr;

            if (_context.Vk.CreateShaderModule(_context.Device, in createInfo, null, out shaderModule) != Result.Success)
            {
                throw new Exception();
            }
        }

        return shaderModule;
    }

    private unsafe byte[] CompileShaderFile(string shaderPath, string entryPoint, ShaderKind shaderKind, bool optimize = true)
    {
        Shaderc? shaderc = null;
        Compiler* compiler = null;
        CompileOptions* compileOptions = null;
        CompilationResult* compilationResult = null;
        try
        {
            _fileWriteTimes.Clear();
            _fileWriteTimes.Add((shaderPath, File.GetLastWriteTime(shaderPath)));
            
            string shaderText = File.ReadAllText(shaderPath);
            byte[] inputFileNameBytes = Encoding.ASCII.GetBytes(Path.GetFileName(shaderPath));
            byte[] entryPointNameBytes = Encoding.ASCII.GetBytes(entryPoint);

            shaderc = Shaderc.GetApi();
            compiler = shaderc.CompilerInitialize();
            compileOptions = shaderc.CompileOptionsInitialize();
            if (optimize)
            {
                shaderc.CompileOptionsSetOptimizationLevel(compileOptions, OptimizationLevel.Performance);
            }
            
            //Set functions for resolving #includes in shaders
            var resolverFn = new PfnIncludeResolveFn(ShaderIncludeResolve);
            var releaseFn = new PfnIncludeResultReleaseFn(ShaderIncludeResultRelease);
            shaderc.CompileOptionsSetIncludeCallbacks(compileOptions, resolverFn, releaseFn, null);
            
            fixed (byte* inputFileName = inputFileNameBytes)
            {
                fixed (byte* entryPointName = entryPointNameBytes)
                {
                    compilationResult = shaderc.CompileIntoSpv(compiler, shaderText, (UIntPtr)shaderText.Length, shaderKind, inputFileName, entryPointName, compileOptions);
                    CompilationStatus compilationStatus = shaderc.ResultGetCompilationStatus(compilationResult);
                    if (compilationStatus != CompilationStatus.Success)
                    {
                        string errorMessage = shaderc.ResultGetErrorMessageS(compilationResult);
                        throw new Exception($"Failed to compile shader: {errorMessage}");
                    }

                    byte* resultBytesPtr = shaderc.ResultGetBytes(compilationResult);
                    UIntPtr resultLength = shaderc.ResultGetLength(compilationResult);
                    Span<byte> bytesSpan = new Span<byte>(resultBytesPtr, (int)resultLength);
                    byte[] resultBytesCopy = bytesSpan.ToArray();

                    return resultBytesCopy;
                }
            }
        }
        catch (Exception ex)
        {
            Log.Error($"Shader compilation error: `{ex.Message}`");
            throw;
        }
        finally
        {
            if (shaderc != null)
            {
                if (compiler != null)
                {
                    shaderc.CompilerRelease(compiler);
                }

                if (compileOptions != null)
                {
                    shaderc.CompileOptionsRelease(compileOptions);
                }

                if (compilationResult != null)
                {
                    shaderc.ResultRelease(compilationResult);
                }

                shaderc.Dispose();
            }
        }
    }
    
    //This and ShaderIncludeReleaseResult() are equivalent to the virtual functions on shaderc::CompileOptions::IncluderInterface in the C++ shaderc API
    private unsafe IncludeResult* ShaderIncludeResolve(void* interfacePtr, byte* requestedSourcePtr, int includeTypeInt, byte* requestingSourcePtr, UIntPtr includeDepthUintPtr)
    {
        string requestedSource = Marshal.PtrToStringAnsi((IntPtr)requestedSourcePtr) ?? throw new Exception("Failed to resolve requestedSource in shader include handler.");
        ulong includeDepth = includeDepthUintPtr;
        if (includeDepth > MaxIncludeDepth)
        {
            Log.Error($"Exceeded maximum include depth of {MaxIncludeDepth} while compiling shader '{Name}'.");
            return null;
        }
        
        string includeFullPath = $"{BuildConfig.ShadersDirectory}{requestedSource}";
        if (requestedSource == Name || _fileWriteTimes.Any(tuple => tuple.path == includeFullPath))
        {
            Log.Error($"Circular include detected when compiling shader '{Name}'.");
            return null;
        }

        if (!File.Exists(includeFullPath))
        {
            Log.Error($"Failed to find shader at include path '{requestedSource}'.");
            return null;
        }

        IncludeResult* includeResult = (IncludeResult*)SilkMarshal.Allocate(Unsafe.SizeOf<IncludeResult>());
        includeResult->SourceName = (byte*)SilkMarshal.StringToPtr(requestedSource);
        includeResult->SourceNameLength = (uint)requestedSource.Length;

        string includedFileContents = File.ReadAllText(includeFullPath);
        includeResult->Content = (byte*)SilkMarshal.StringToPtr(includedFileContents);
        includeResult->ContentLength = (uint)includedFileContents.Length;
        
        _fileWriteTimes.Add((includeFullPath, File.GetLastWriteTime(includeFullPath)));
        return includeResult;
    }

    private unsafe void ShaderIncludeResultRelease(void* interfacePtr, IncludeResult* data)
    {
        SilkMarshal.Free((nint)data->SourceName);
        SilkMarshal.Free((nint)data->Content);
        SilkMarshal.Free((IntPtr)data);
    }

    private ShaderModule LoadAndCompileShaderFile(string shaderPath, string entryPoint, ShaderKind shaderKind, bool optimize = true)
    {
        byte[] compiledShader = CompileShaderFile(shaderPath, entryPoint, shaderKind, optimize);
        ShaderModule shaderModule = CreateShaderModule(compiledShader);
        return shaderModule;
    }

    public unsafe PipelineShaderStageCreateInfo GetPipelineStageInfo()
    {
        ShaderStageFlags shaderStageFlags = Kind switch
        {
            ShaderKind.VertexShader => ShaderStageFlags.VertexBit,
            ShaderKind.FragmentShader => ShaderStageFlags.FragmentBit,
            _ => throw new Exception($"Unsupported shader kind {Kind}")
        };

        PipelineShaderStageCreateInfo shaderStageCreateInfo = new()
        {
            SType = StructureType.PipelineShaderStageCreateInfo,
            Stage = shaderStageFlags,
            Module = Module,
            PName = (byte*)SilkMarshal.StringToPtr(EntryPoint)
        };
        
        return shaderStageCreateInfo;
    }

    public unsafe void Destroy()
    {
        _context.Vk.DestroyShaderModule(_context.Device, Module, null);
    }
}