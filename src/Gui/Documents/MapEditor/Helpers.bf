using System;
using System.Collections;
using Nanoforge.FileSystem;
using Xml_Beef;
using ImGui;
using Nanoforge.Misc;
namespace Nanoforge.Gui.Documents.MapEditor;

public class XtblComboBox
{
    private append String _label;
    private append String _xtbl;
    private append String _xtblFieldPath;
    private append List<String> _options ~ClearAndDeleteItems(_);
    private append String _searchTerm;
    private bool _loaded = false;
    private bool _loadError = false;

    public this(StringView label, StringView xtbl, StringView xtblFieldPath)
    {
        _label..Set(label).EnsureNullTerminator();
        _xtbl.Set(xtbl);
        _xtblFieldPath.Set(xtblFieldPath);
    }

    public void Draw(String text)
    {
        if (!_loaded && !_loadError)
        {
            if (InspectorHelpers.LoadOptionsFromXtbl(_options, _xtbl, _xtblFieldPath))
                _loaded = true;
            else
                _loadError = true;
        }

        if (_loadError)
        {
            ImGui.TextColored(.Red, scope $"Failed to load '{_xtblFieldPath}' in {_xtbl}. Check the log.");
            return;
        }

        text.EnsureNullTerminator();
        if (ImGui.BeginCombo(_label, text.Ptr))
        {
            ImGui.InputText("Search", _searchTerm);
            ImGui.Separator();

            for (String option in _options)
            {
                if (!_searchTerm.IsEmpty && !option.Contains(_searchTerm, true))
                    continue;

                bool selected = (option.Equals(text));
                if (ImGui.Selectable(option, selected))
                {
                    //TODO: Add change tracking
                    text.Set(option);
                }
            }
            ImGui.EndCombo();
        }
    }
}

public class EnumComboBox<T> where T : enum
{
    private append String _label;
    private append List<(String, T)> _options;
    private append String _searchTerm;
    private bool _loaded = false;

    public this(StringView label)
    {
        _label..Set(label).EnsureNullTerminator();
    }

    public ~this()
    {
        for (var option in _options)
            delete option.0;

        _options.Clear();
    }

    public void Draw(ref T value)
    {
        if (!_loaded)
        {
            for (var field in typeof(T).GetFields())
            {
                //TODO: Make this cleaner. Try using GetValue
                var markerType = (T)field.[Friend]mFieldData.mData;
                _options.Add((new String(field.Name)..EnsureNullTerminator(), markerType));
            }
            _loaded = true;
        }

        String currentValueText = value.ToString(.. scope .())..EnsureNullTerminator();
        if (ImGui.BeginCombo(_label, currentValueText.Ptr))
        {
            ImGui.InputText("Search", _searchTerm);
            ImGui.Separator();

            for (var option in _options)
            {
                StringView optionText = option.0;
                T optionValue = option.1;
                if (!_searchTerm.IsEmpty && !optionText.Contains(_searchTerm, true))
	                continue;

                bool selected = optionText.Equals(currentValueText);
                if (ImGui.Selectable(optionText, selected))
                {
                    value = optionValue;
                }
            }
            ImGui.EndCombo();
        }
    }
}

static class InspectorHelpers
{
    public static bool LoadOptionsFromXtbl(List<String> options, StringView xtblName, StringView fieldPath)
    {
        String xtblPath = scope $"//data/misc.vpp_pc/{xtblName}";
        switch (PackfileVFS.ReadAllText(xtblPath))
        {
            case .Ok(String text):
                defer delete text;
                Xml xml = scope .();
                xml.LoadFromString(text); //TODO: Add error handling. Looks like I might need to modify the XML lib

                XmlNode root = xml.ChildNodes[0];
                PopulateXtblOptions(root, fieldPath, options); //Recurse through field path adding all instances of it to options list
                return true;
            case .Err:
                Logger.Error("Failed to load {0} for inspector property options.", xtblPath);
                return false;
        }
    }

    private static void PopulateXtblOptions(XmlNode node, StringView fieldPath, List<String> options)
    {
        int nextPart = fieldPath.IndexOf(';');
        if (nextPart == -1)
        {
            //We've reach the end of the field path. Now just read the field
            XmlNodeList subnodes = node.FindNodes(scope String(fieldPath));
            defer delete subnodes;
            for (XmlNode field in subnodes) //TODO: Modify library to take StringView here
            {
                options.Add(new String()..Set(field.NodeValue));
            }
        }
        else
        {
            //Still have some parts of the path to go. Keep recursing by finding the node at the front of the path
            StringView currentNodeName = fieldPath.Substring(0, nextPart);
            StringView newPath = fieldPath.Substring(nextPart + 1);
            XmlNodeList subnodes = node.FindNodes(scope String(currentNodeName));
            defer delete subnodes;
            for (XmlNode newNode in subnodes)
            {
                PopulateXtblOptions(newNode, newPath, options);
            }
        }
    }
}