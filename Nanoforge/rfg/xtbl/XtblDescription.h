#pragma once
#include "Common/Typedefs.h"
#include "XtblType.h"
#include "Common/string/String.h"
#include <tinyxml2/tinyxml2.h>
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
    XtblType Type;
    string DisplayName;
    string Description;
    bool Required = true;
    bool Unique = false;
    string Default;
    i32 MaxCount;

    Handle<XtblDescription> Parent = nullptr;
    std::vector<Handle<XtblDescription>> Subnodes;
    std::vector<string> Choices;
    string DefaultChoice;
    std::vector<string> Flags;
    Handle<XtblReference> Reference = nullptr;
    string Extension;
    string StartingPath;
    bool ShowPreload;
    f32 Min;
    f32 Max;
    i32 MaxChildren;
    i32 NumDisplayRows;

    bool Parse(tinyxml2::XMLElement* node, Handle<XtblDescription> self, XtblFile& file);
    //Returns the path of the value. This is this nodes name prepended with the names of it's parents
    string GetPath();
};