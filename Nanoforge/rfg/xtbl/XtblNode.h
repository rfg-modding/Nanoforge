#pragma once
#include "Common/Typedefs.h"
#include "XtblType.h"
#include "RfgTools++/types/Vec3.h"
#include "RfgTools++/types/Vec4.h"
#include <tinyxml2/tinyxml2.h>
#include <vector>
#include <optional>
#include <variant>

class XtblFile;
struct XtblFlag
{
    string Name;
    bool Value;
};
using XtblValue = std::variant<string, i32, f32, Vec3, u32, XtblFlag>;

//Represent an xml element in an xtbl file
class XtblNode
{
public:
    string Name;
    XtblValue Value;
    XtblType Type = XtblType::None;
    std::vector<Handle<XtblNode>> Subnodes;
    Handle<XtblNode> Parent = nullptr;
    bool CategorySet = false; //Used by XtblFile to track which nodes are uncategorized
    bool Enabled = true; //Whether or not the node should be included in the file xtbl (for non-required nodes)
    bool HasDescription = false; //Whether or not the XtblNode has a description. Either in the <TableDescription> block or one provided by modders

    bool HasValue() { return Subnodes.size() == 0; }
    bool HasSubnodes() { return Subnodes.size() != 0; }
    bool Parse(tinyxml2::XMLElement* node, XtblFile& xtbl, Handle<XtblNode> self);
    std::optional<string> GetSubnodeValueString(const string& subnodeName);
    Handle<XtblNode> GetSubnode(const string& subnodeName);
    //Returns the path of the value. This is this nodes name prepended with the names of it's parents
    string GetPath();
};