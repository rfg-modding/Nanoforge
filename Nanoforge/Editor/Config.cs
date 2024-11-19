
namespace Nanoforge.Editor;

/// <summary>
/// CVars are an easy way to define global EditorObjects without needing to manually add & retrieve them from NanoDB.
/// Just define a field/property of the type CVar<T> where T is an EditorObject you want to persist across NanoDB sessions and projects.
/// When the instance is it'll handle loading its value from NanoDB and creating a global EditorObject for the data if it doesn't already exist.    
/// </summary>
public class CVar<T> where T : EditorObject, new()
{
    public T Value { get; private set; }
    public readonly string Name;

    public CVar(string name)
    {
        Name = name;
        if (!NanoDB.LoadedGlobalObjects)
        {
            NanoDB.LoadGlobals();
        }

        T? value = NanoDB.Find<T>(Name);
        if (value == null)
        {
            Value = NanoDB.CreateGlobalObject<T>(Name);
            NanoDB.SaveGlobals();   
        }
        else
        {
            Value = value;
        }
    }
    
    //Save the variable to Settings.nanodata. Note: Really saves all the CVars since ProjectDB doesn't support selective saves. So don't spam this.
    public void Save()
    {
        NanoDB.SaveGlobals();
    }
}