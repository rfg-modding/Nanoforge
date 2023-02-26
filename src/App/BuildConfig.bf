using System;

namespace Nanoforge.App
{
	public static class BuildConfig
	{
		public static readonly String ProjectName = new String() ~DeleteIfSet!(_);
		public static readonly String AssetsBasePath = new String() ~DeleteIfSet!(_);
        public static readonly String ShadersPath = new String() ~DeleteIfSet!(_);
        public static readonly String Version = new String() ~DeleteIfSet!(_);

		public static this()
		{
            ProjectName.Set("Nanoforge");
#if DEBUG
            AssetsBasePath.Set(scope $@"{Workspace.Directory}\assets\");
#else
            AssetsBasePath.Set(@".\assets\");
#endif
            ShadersPath.Set(scope $@"{AssetsBasePath}shaders\");
            Version.Set("v1.0.0_pre0");
		}
	}
}