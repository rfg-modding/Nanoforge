using Nanoforge.Rfg;
using Nanoforge.App;
using ImGui;
using System;
using System.Collections;
using Nanoforge.FileSystem;
using Common;
using Nanoforge.Misc;
using Xml_Beef;
using System.IO;
using System.Reflection;
using static Nanoforge.Rfg.MultiMarker;
namespace Nanoforge.Gui.Documents.MapEditor;

//Inspector for a specific zone object type. Implemented as a separate class to keep a clean separation between data and UI logic.
public interface IZoneObjectInspector<T> where T : ZoneObject
{
    public static void Draw(App app, Gui gui, T obj);
}

public static class BaseZoneObjectInspector : IZoneObjectInspector<ZoneObject>
{
    public static void Draw(App app, Gui gui, ZoneObject obj)
    {
        FontManager.FontL.Push();
        ImGui.Text(obj.GetType().GetName(.. scope .()));
        FontManager.FontL.Pop();

        //TODO: Look at NF++ implementation and show/hide/group fields the same way here
        ImGui.InputScalar("Handle", .U32, &obj.Handle);
        ImGui.InputScalar("Num", .U32, &obj.Num);
        //TODO: Implement remaining fields
    }
}

public static class ObjZoneInspector : IZoneObjectInspector<ObjZone>
{
    private static append XtblComboBox _ambientSpawnCombo = .("Ambient spawn", "ambient_spawn_info.xtbl", "table;ambient_spawn_info;Name");

    public static void Draw(App app, Gui gui, ObjZone obj)
    {
        BaseZoneObjectInspector.Draw(app, gui, obj);
        _ambientSpawnCombo.Draw(obj.AmbientSpawn);
        ImGui.InputText("Spawn resource", obj.SpawnResource);
        ImGui.InputText("Terrain file name", obj.TerrainFileName);
        ImGui.InputFloat("Wind min speed", &obj.WindMinSpeed);
        ImGui.InputFloat("Wind max speed", &obj.WindMaxSpeed);
    }
}

public static class MultiMarkerInspector : IZoneObjectInspector<MultiMarker>
{
    private static append EnumComboBox<MultiMarkerType> _markerTypeCombo = .("Marker Type");

    public static void Draw(App app, Gui gui, MultiMarker obj)
    {
        BaseZoneObjectInspector.Draw(app, gui, obj);

        _markerTypeCombo.Draw(ref obj.MarkerType);
        //TODO: Implement remaining fields
    }
}

//TODO: Implement other inspectors

