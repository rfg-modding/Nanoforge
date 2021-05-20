#pragma once
#include "Common/Typedefs.h"
#include "XtblType.h"
#include "RfgTools++/types/Vec3.h"
#include "RfgTools++/types/Vec4.h"
#include <tinyxml2/tinyxml2.h>
#include <vector>
#include <optional>
#include <variant>

class GuiState;
class XtblFile;
struct XtblFlag
{
    string Name;
    bool Value;
};
using XtblValue = std::variant<string, i32, f32, Vec3, u32, XtblFlag>;

//Interface used by all xtbl nodes. Includes data that all nodes have. Separate implementations must be written for each node data type (e.g. float, int, reference, etc)
class IXtblNode
{
public:
    //IXtblNode interface functions
    virtual ~IXtblNode() {}
    //Draw editor for node using Dear ImGui
    virtual void DrawEditor(GuiState* guiState, Handle<XtblFile> xtbl, Handle<IXtblNode> rootNode, const char* nameOverride = nullptr) = 0;
    //Initialize node with default values for it's data type
    virtual void InitDefault() = 0;

    //Functions that all nodes have
    bool HasValue() { return Subnodes.size() == 0; }
    bool HasSubnodes() { return Subnodes.size() != 0; }
    void DeleteSubnodes();
    std::optional<string> GetSubnodeValueString(const string& subnodeName);
    Handle<IXtblNode> GetSubnode(const string& subnodeName);
    //Returns the path of the value. This is this nodes name prepended with the names of it's parents
    string GetPath();

    //Values that all nodes have
    string Name;
    XtblValue Value;
    XtblType Type = XtblType::None;
    std::vector<Handle<IXtblNode>> Subnodes;
    Handle<IXtblNode> Parent = nullptr;
    bool CategorySet = false; //Used by XtblFile to track which nodes are uncategorized
    bool Enabled = true; //Whether or not the node should be included in the file xtbl (for non-required nodes)
    bool HasDescription = false; //Whether or not the XtblNode has a description. Either in the <TableDescription> block or one provided by modders
    bool Edited = false; //Set to true if edited
};