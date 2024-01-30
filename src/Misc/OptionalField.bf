using Common;
using System;
using Nanoforge.App;

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
            //Don't try deleting these. NanoDB owns them and is responsible for deleting them.
            if (typeof(T) == typeof(EditorObject) || typeof(T) == typeof(ProjectBuffer))
                return;

            if (Value != null)
            {
                delete Value;
            }
        }

        public void SetAndEnable(T value)
        {
            Value = value;
            Enabled = true;
        }
    }
}