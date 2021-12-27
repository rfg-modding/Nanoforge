#pragma once
#include "Common/Typedefs.h"
#include "common/Handle.h"
#include "XtblType.h"
#include <optional>
#include <vector>

namespace tinyxml2
{
    class XMLElement;
}
class XtblFile;

//Reference to another xtbl
class XtblReference
{
public:
    bool Used = false;
    string File;
    string Path;
    bool OpenSeparate;
};

//Description of an xtbl element. Read from <TableDescription></TableDescription> block
class XtblDescription
{
public:
    string Name;
    XtblType Type = XtblType::None;
    string DisplayName;
    std::optional<string> Description;
    std::optional<bool> Required = true;
    std::optional<bool> Unique = false;
    std::optional<string> Default;
    std::optional<i32> MaxCount;

    Handle<XtblDescription> Parent = nullptr;
    std::vector<Handle<XtblDescription>> Subnodes;
    std::vector<string> Choices;
    string DefaultChoice;
    std::vector<string> Flags;
    Handle<XtblReference> Reference = nullptr;
    std::optional<string> Extension;
    std::optional<string> StartingPath;
    std::optional<bool> ShowPreload;
    std::optional<f32> Min = {};
    std::optional<f32> Max = {};
    std::optional<i32> MaxChildren;
    std::optional<i32> NumDisplayRows;

    //Parse description from xml
    bool Parse(tinyxml2::XMLElement* node, Handle<XtblDescription> self, XtblFile& file);
    //Write description to xml
    bool WriteXml(tinyxml2::XMLElement* xml);
    //Returns the path of the value. This is this nodes name prepended with the names of it's parents
    string GetPath();
};