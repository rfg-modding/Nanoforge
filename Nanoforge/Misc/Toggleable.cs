namespace Nanoforge.Misc;

//Used to represent values that can get toggled in the UI. Disabled values don't get written to the zone files when a map is exported. The value is preserved when enabled/disabled.
public class Toggleable<T>
{
    public T Value;
    public bool Enabled;

    public Toggleable(T value, bool enabled = false)
    {
        Value = value;
    }

    public void SetAndEnable(T value)
    {
        Value = value;
        Enabled = true;
    }
}