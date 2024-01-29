using System;
using Common;
using Nanoforge.Misc;
using System.Collections;
using Bon;
namespace Nanoforge.App;

static
{
    public static List<ICVar> CVarsRequiringInitialization = new .() ~delete _; //Used as workaround to static init problems. See comments on ICVar
    public static CVar<GeneralSettings> CVar_GeneralSettings = new .("GeneralSettings") ~delete _; 
}

//General settings for NF that fit in no particular category
[BonTarget, ReflectAll]
public class GeneralSettings : EditorObject
{
    public String DataPath = new .() ~delete _;
    public List<String> RecentProjects = new .() ~DeleteContainerAndItems!(_);
    public String NewProjectDirectory = new .() ~delete _; //So you don't have to keep picking the folder every time you make a project
}

public interface ICVar
{
    //Used as workaround to static init issues. Even though [StaticInitAfter(typeof(ProjectDB))] was set on CVar<T> the constructor of static CVar<T> instances was being run prior to ProjectDB static init.
    //TODO: Not sure if that's the intended behavior of that attribute. Make a minimal example project and make an issue on the Beef github asking about this.
    public void Init();
}

//Use to register a global object in ProjectDB. It'll automatically create the object if it doesn't already exist and provides an easier way to access it.
public class CVar<T> : ICVar
                       where T : new, delete, class, EditorObject
{
    public T Value = null;
    public append String Name;

    public this(StringView cvarName)
    {
        Name.Set(scope $"CVar_{cvarName}");
        CVarsRequiringInitialization.Add(this);
    }

    ///Save the variable to Settings.nanodata. Note: Really saves all the CVars since ProjectDB doesn't support selective saves. So don't spam this.
    public void Save()
    {
        NanoDB.SaveGlobals();
    }

    public static T operator->(Self self)
    {
        return self.Value;
    }

    void ICVar.Init()
    {
        if (!NanoDB.LoadedGlobalObjects)
        {
            NanoDB.LoadGlobals();
        }

        Value = (T)NanoDB.Find(Name);
        if (Value == null)
        {
            //If CVar EditorObject doesn't exist create it.
            Value = NanoDB.CreateGlobalObject<T>(Name);
        }
    }
}