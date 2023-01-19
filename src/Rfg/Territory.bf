using Nanoforge.App;
using Nanoforge;
using System;

namespace Nanoforge.Rfg
{
    //Root structure of any RFG map. Each territory is split into one or more zones which contain the map objects.
	public class Territory : EditorObject
	{
        public Result<void, StringView> Load()
        {
            return .Ok;
        }
	}
}