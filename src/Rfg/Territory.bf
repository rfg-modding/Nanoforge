using System.Collections;
using Nanoforge.App;
using Common.Math;
using Common;
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