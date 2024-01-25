using Common;
using System;

namespace Nanoforge.Misc
{
    //For zone object properties that can be toggled in the map editor. Nanoforge still keeps track of the data for disabled fields, but it doesn't write them to RFG files on export.
    [ReflectAll]
	public struct OptionalValue<T> where T : struct
	{
        public T Value;
        public bool Enabled;

        public this(T value, bool enabled = false)
        {
            Value = value;
            Enabled = enabled;
        }

        public void SetAndEnable(T value) mut
        {
            Value = value;
            Enabled = true;
        }
	}

    [ReflectAll]
    public class OptionalObject<T> where T : Object, delete
    {
        public T Value;
        public bool Enabled;

        public this(T value, bool enabled = false)
        {
            Value = value;
            Enabled = enabled;
        }

        public ~this()
        {
            delete Value;
        }

        
        public void SetAndEnable(T value)
        {
            Value = value;
            Enabled = true;
        }
    }
}