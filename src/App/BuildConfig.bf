using System;

namespace Nanoforge.App
{
	public class BuildConfig
	{
		public append readonly String ProjectName;
		public append readonly String AssetsBasePath;
        public append readonly String Version;

		public this()
		{

		}

		public this(StringView projectName, StringView assetsBasePath, StringView version)
		{
			ProjectName.Set(projectName);
			AssetsBasePath.Set(assetsBasePath);
            Version.Set(version);
		}
	}
}