using System;

namespace Nanoforge.App
{
	public class BuildConfig
	{
		public String ProjectName = new .() ~delete _;
		public String AssetsBasePath = new .() ~delete _;

		public this()
		{

		}

		public this(StringView projectName, StringView assetsBasePath)
		{
			ProjectName.Set(projectName);
			AssetsBasePath.Set(assetsBasePath);
		}
	}
}