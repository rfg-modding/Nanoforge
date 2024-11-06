using System;

namespace Nanoforge.Editor;

public class EditorObject
{
    // ReSharper disable once InconsistentNaming
    public const ulong NullUID = ulong.MaxValue;

    // ReSharper disable once InconsistentNaming
    public ulong UID { get; private set; } = NullUID;

    public string Name = "";

    public virtual EditorObject Clone()
    {
        EditorObject clone = (EditorObject)MemberwiseClone();
        clone.UID = NullUID; //Default to NullUID so it's clear in the debugger that the object isn't valid yet. Gets a valid UID when finally added to NanoDB.
        clone.Name = new string(Name);
        return clone;
    }
}