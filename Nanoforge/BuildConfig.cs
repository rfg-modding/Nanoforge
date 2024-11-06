
namespace Nanoforge;

public static class BuildConfig
{
    public static readonly string ProjectName;
    public static readonly string AssetsBasePath;
    public static readonly string ShadersPath;
    public static readonly string Version;
    public static readonly bool EnableGraphicsDebugFeatures = false;

    static BuildConfig()
    {
        ProjectName = "Nanoforge";
#if DEBUG
        //TODO: Porting fix: Figure out what the equivalent of this is for Dotnet. I want it to load the assets from the dev project directory for convenience when editing shaders.
        AssetsBasePath = "/home/moneyl/projects/Nanoforge/assets/";
        //AssetsBasePath.Set(scope $@"{Workspace.Directory}\assets\");
#else
        AssetsBasePath = "./assets/";
#endif
        ShadersPath = $@"{AssetsBasePath}shaders/";
        Version = "v1.1.0";
    }
}