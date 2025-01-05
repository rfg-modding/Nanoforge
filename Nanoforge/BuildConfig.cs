
using System;
using System.IO;

namespace Nanoforge;

public static class BuildConfig
{
    public static readonly string ProjectName;
    public static readonly string AssetsDirectory;
    public static readonly string ShadersDirectory;
    public static readonly string Version;
    public static readonly bool EnableGraphicsDebugFeatures = false;

    static BuildConfig()
    {
        ProjectName = "Nanoforge";
#if DEBUG
        string workingDirectory = Environment.CurrentDirectory;
        string? projectDir = Directory.GetParent(workingDirectory)?.Parent?.Parent?.FullName;
        
        AssetsDirectory = $"{projectDir}/assets/";
#else
        AssetsBasePath = "./assets/";
#endif
        ShadersDirectory = $@"{AssetsDirectory}shaders/";
        Version = "v2.0.0";
    }
}