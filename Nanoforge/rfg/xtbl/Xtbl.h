#pragma once
#include "Common/Typedefs.h"
#include "XtblType.h"
#include "IXtblNode.h"
#include "XtblDescription.h"
#include <unordered_map>
#include <vector>

//Categories specified by <_Editor><Category>...</Category></_Editor> blocks
class XtblCategory
{
public:
    XtblCategory(s_view name) : Name(name) {}

    string Name;
    std::vector<Handle<XtblCategory>> SubCategories = {};
    std::vector<Handle<IXtblNode>> Nodes = {};
};

//Represents an xtbl file. This intermediate form is used instead of directly interacting with xml since it's more convenient
class XtblFile
{
public:
    ~XtblFile();

    //Generate XtblNode tree by parsing an xtbl file
    bool Parse(const string& path);
    //Parse xtbl node. Returns an IXtblNode instance if successful. Returns nullptr if it fails.
    Handle<IXtblNode> ParseNode(tinyxml2::XMLElement* node, Handle<IXtblNode> parent, string& path);
    //Get description of xtbl value
    Handle<XtblDescription> GetValueDescription(const string& valuePath, Handle<XtblDescription> desc = nullptr);
    //Add node to provided category. Category will be created if it doesn't already exist
    void SetNodeCategory(Handle<IXtblNode> node, string categoryPath);
    //Get path of category that the node is in. E.g. Entries:EDF
    string GetNodeCategory(Handle<IXtblNode> node);
    //Get category and create it if it doesn't already exist
    Handle<XtblCategory> GetOrCreateCategory(s_view categoryPath, Handle<XtblCategory> parent = nullptr);
    //Get subnodes of search node
    std::vector<Handle<IXtblNode>> GetSubnodes(const string& nodePath, Handle<IXtblNode> node, std::vector<Handle<IXtblNode>>* nodes = nullptr);
    //Get subnode of search node
    Handle<IXtblNode> GetSubnode(const string& nodePath, Handle<IXtblNode> node);
    //Get root node by <Name> subnode value
    Handle<IXtblNode> GetRootNodeByName(const string& name);
    //Ensure elements of provided description exist on XtblNode. If enableOptionalSubnodes = false then optional subnodes will be created but disabled.
    void EnsureEntryExists(Handle<XtblDescription> desc, Handle<IXtblNode> node, bool enableOptionalSubnodes = true);
    //Write all nodes to xtbl file
    void WriteXtbl(const string& outPath);

    //Filename of the xtbl
    string Name;
    //Name of the vpp_pc file this xtbl is in
    string VppName;
    //Xml nodes inside the <Table></Table> block
    std::vector<Handle<IXtblNode>> Entries;
    //Xml nodes inside the <TableTemplates></TableTemplates> block
    std::vector<Handle<IXtblNode>> Templates;
    //Xml nodes inside the <TableDescription></TableDescription> block. Describes data in <Table>
    Handle<XtblDescription> TableDescription = CreateHandle<XtblDescription>();
    //Root category
    Handle<XtblCategory> RootCategory = CreateHandle<XtblCategory>("Entries");
    //References to other xtbl files
    std::vector<Handle<XtblReference>> References;

private:
    //Easy way to get a nodes category with needing to search the category tree
    std::unordered_map<Handle<IXtblNode>, string> categoryMap_;
};