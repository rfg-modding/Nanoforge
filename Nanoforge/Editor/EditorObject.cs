using System.Text.Json.Serialization;

namespace Nanoforge.Editor;

public class EditorObject
{
    // ReSharper disable once InconsistentNaming
    public const ulong NullUID = ulong.MaxValue;

    [JsonInclude]
    private ulong _uid = NullUID;
    
    // ReSharper disable once InconsistentNaming
    [JsonIgnore]
    public ulong UID
    {
        get => _uid;
        private set
        {
            _uid = value;    
        }
        
    }

    public string Name = "";

    public virtual EditorObject Clone()
    {
        EditorObject clone = (EditorObject)MemberwiseClone();
        clone.UID = NullUID; //Default to NullUID so it's clear in the debugger that the object isn't valid yet. Gets a valid UID when finally added to NanoDB.
        clone.Name = new string(Name);
        return clone;
    }

    //Do NOT call this unless you know what you're doing. This is really only for NanoDB to use when creating objects.
    //I made it into a function instead of using the property so it's hard to accidentally change the UID.
    public void SetUID(ulong uid)
    {
        UID = uid;
    }
}