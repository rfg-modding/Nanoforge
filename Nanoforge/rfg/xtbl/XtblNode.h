#pragma once
#include "Common/Typedefs.h"
#include "XtblType.h"
#include "RfgTools++/types/Vec3.h"
#include "RfgTools++/types/Vec4.h"
#include <tinyxml2/tinyxml2.h>
#include <vector>
#include <optional>

class XtblFile;

union XtblValue
{
    XtblValue() { }
    ~XtblValue() { } //Todo: Will this cause a memory leak if XtblValue is a string?

    string String;
    i32 Int;
    f32 Float;
    Vec3 Vector;
    Vec3 Color;
    u32 Flags;
};

//Represent an xml element in an xtbl file
class XtblNode
{
public:
    string Name;
    XtblValue Value;
    XtblType Type = XtblType::None;
    std::vector<Handle<XtblNode>> Subnodes;
    Handle<XtblNode> Parent = nullptr;

    bool HasValue() { return Subnodes.size() == 0; }
    bool HasSubnodes() { return Subnodes.size() != 0; }
    bool Parse(tinyxml2::XMLElement* node, XtblFile& xtbl, Handle<XtblNode> self);
    std::optional<string> GetSubnodeValueString(const string& subnodeName);
    //Returns the path of the value. This is this nodes name prepended with the names of it's parents
    string GetPath();
};