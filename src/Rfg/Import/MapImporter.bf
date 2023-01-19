using Nanoforge.App;
using Nanoforge;
using System;

namespace Nanoforge.Rfg.Import
{
	public static class MapImporter
	{
        //Import all assets used by an RFG territory
        public static Result<Territory, StringView> ImportMap(StringView name)
        {
            Territory map = ProjectDB.CreateObject<Territory>(name);

            return .Ok(map);
        }
	}
}