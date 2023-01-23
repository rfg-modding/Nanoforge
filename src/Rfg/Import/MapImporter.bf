using Nanoforge.App.Project;
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
            using (var changes = BeginCommit!(scope $"Import map - {name}"))
            {
                Territory map = changes.CreateObject<Territory>(name);


                return .Ok(map);
            }
        }
	}
}