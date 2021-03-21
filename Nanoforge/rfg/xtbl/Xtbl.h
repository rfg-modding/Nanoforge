#pragma once
#include "Common/Typedefs.h"
#include "XtblType.h"
#include "XtblNode.h"
#include "XtblDescription.h"
#include <vector>

//Categories specified by <_Editor><Category>...</Category></_Editor> blocks
class XtblCategory
{
public:
    XtblCategory(s_view name) : Name(name) {}

    string Name;
    std::vector<Handle<XtblCategory>> SubCategories = {};
    std::vector<Handle<XtblNode>> Nodes = {};
};

//Represents an xtbl file. This intermediate form is used instead of directly interacting with xml since it's more convenient
class XtblFile
{
public:
    //Generate XtblNode tree by parsing an xtbl file
    bool Parse(const string& path);
    //Get description of xtbl value
    std::optional<XtblDescription> GetValueDescription(const string& valuePath);
    //Add node to provided category. Category will be created if it doesn't already exist
    void SetNodeCategory(Handle<XtblNode> node, s_view categoryPath);
    //Get category and create it if it doesn't already exist
    Handle<XtblCategory> GetOrCreateCategory(s_view categoryPath, Handle<XtblCategory> parent = nullptr);

    //Filename of the xtbl
    string Name;
    //Name of the vpp_pc file this xtbl is in
    string VppName;
    //Xml nodes inside the <Table></Table> block
    std::vector<Handle<XtblNode>> Entries;
    //Xml nodes inside the <TableTemplates></TableTemplates> block
    std::vector<Handle<XtblNode>> Templates;
    //Xml nodes inside the <TableDescription></TableDescription> block. Describes data in <Table>
    XtblDescription TableDescription;
    //Root category
    Handle<XtblCategory> RootCategory = CreateHandle<XtblCategory>("Entries");
};