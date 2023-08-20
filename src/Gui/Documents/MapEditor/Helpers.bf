#pragma warning disable 168
using System;
using System.Collections;
using Nanoforge.FileSystem;
using Xml_Beef;
using ImGui;
using Nanoforge.Misc;
using Common;
using System.Linq;
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

    public this(StringView label, StringView xtbl, StringView xtblFieldPath, StringView defaultOption = "")
    {
        _label..Set(label).EnsureNullTerminator();
        _xtbl.Set(xtbl);
        _xtblFieldPath.Set(xtblFieldPath);

        //Some fields have a default value that isn't in the xtbls. E.g. PlayerStart.MissionInfo is set to none in a lot of vanilla MP maps
        if (defaultOption != "")
            _options.Add(new String(defaultOption));
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
    private (T, StringView)[] _nameOverrides ~DeleteIfSet!(_);
    private bool _loaded = false;

    public this(StringView label, (T, StringView)[] nameOverrides = null)
    {
        _label..Set(label).EnsureNullTerminator();
        _nameOverrides = nameOverrides;
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
                var enumValue = (T)field.[Friend]mFieldData.mData;
                String enumText = new String(field.Name)..EnsureNullTerminator();

                //Use override if present
                if (_nameOverrides != null)
                {
                    for (var kv in _nameOverrides)
                    {
                        if (kv.0 == enumValue)
                        {
							enumText.Set(kv.1);
                            break;
						}
                    }
                }

                _options.Add((enumText, enumValue));
            }
            _loaded = true;
        }

        String currentValueText = GetOptionText(value).GetValueOrDefault("")..EnsureNullTerminator();
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

    private Result<String> GetOptionText(T value)
    {
        for (var kv in _options)
        {
            if (kv.1 == value)
            {
                return .Ok(kv.0);
            }
        }

        return .Err;
    }
}

//Assumes T is an enum where each option is set to a multiple of 2 (a specific bit)
public class BitflagComboBox<T> where T : enum
{
    private append String _label;
    private append List<(String, T)> _options;
    private (T, StringView)[] _nameOverrides ~DeleteIfSet!(_);
    private bool _loaded = false;

    public this(StringView label, (T, StringView)[] nameOverrides = null)
    {
        _label..Set(label).EnsureNullTerminator();
        _nameOverrides = nameOverrides;
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
                var enumValue = (T)field.[Friend]mFieldData.mData;
                if (enumValue == 0)
                    continue; //Ignore "None" flags

                String enumText = new String(field.Name)..EnsureNullTerminator();

                //Use override if present
                if (_nameOverrides != null)
                {
                    for (var kv in _nameOverrides)
                    {
                        if (kv.0 == enumValue)
                        {
                			enumText.Set(kv.1);
                            enumText.EnsureNullTerminator();
                            break;
                		}
                    }
                }

                _options.Add((enumText, enumValue));
            }
            _loaded = true;
        }

        if (ImGui.CollapsingHeader(_label))
        {
            const f32 indent = 15.0f;
            ImGui.Indent(indent);
            for (var option in _options)
            {
                int valueInt = (int)value;
                StringView optionText = option.0;
                int optionValue = (int)option.1;

                bool bitEnabled = (valueInt & optionValue) != 0;
                ImGui.Checkbox(optionText.Ptr, &bitEnabled);
                if (bitEnabled)
                {
                    //Checked, set this bit to true
                    int newValue = valueInt | optionValue;
                    value = (T)newValue;
                }
                else
                {
                    //Unchecked, zero this bit
                    int newValue = valueInt & (~optionValue);
                    value = (T)newValue;
                }
            }
            ImGui.Unindent(indent);
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