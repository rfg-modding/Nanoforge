#pragma once
#include "Common/Typedefs.h"
#include "common/Handle.h"
#include "common/String.h"
#include <unordered_map>
#include <vector>

class PackfileVFS;
class IXtblNode;
class XtblDescription;
class XtblReference;
namespace tinyxml2
{
    class XMLElement;
    class XMLDocument;
}

//Categories specified by <_Editor><Category>...</Category></_Editor> blocks
class XtblCategory
{
public:
    XtblCategory(s_view name) : Name(name) {}

    string Name;
    std::vector<Handle<XtblCategory>> SubCategories = {};
    std::vector<IXtblNode*> Nodes = {};
};

//Represents an xtbl file. This intermediate form is used instead of directly interacting with xml since it's more convenient
class XtblFile
{
public:
    XtblFile();
    ~XtblFile();

    //Reparse xtbl file
    bool Reload(PackfileVFS* packfileVFS);
    //Generate XtblNode tree by parsing an xtbl file
    bool Parse(const string& path, PackfileVFS* packfileVFS);
    //Parse xtbl node. Returns an IXtblNode instance if successful. Returns nullptr if it fails.
    IXtblNode* ParseNode(tinyxml2::XMLElement* node, IXtblNode* parent, string& path);
    //Get description of xtbl value
    Handle<XtblDescription> GetValueDescription(const string& valuePath, Handle<XtblDescription> desc = nullptr);
    //Add node to provided category. Category will be created if it doesn't already exist
    void SetNodeCategory(IXtblNode* node, string categoryPath);
    //Get path of category that the node is in. E.g. Entries:EDF
    string GetNodeCategoryPath(IXtblNode* node);
    //Get Handle<XtblCategory> to category that the node is in
    Handle<XtblCategory> GetNodeCategory(IXtblNode* node);
    //Get category and create it if it doesn't already exist
    Handle<XtblCategory> GetOrCreateCategory(s_view categoryPath, Handle<XtblCategory> parent = nullptr);
    //Get subnodes of search node
    std::vector<IXtblNode*> GetSubnodes(const string& nodePath, IXtblNode* node, std::vector<IXtblNode*>* nodes = nullptr);
    //Get subnode of search node
    IXtblNode* GetSubnode(const string& nodePath, IXtblNode* node);
    //Get root node by <Name> subnode value
    IXtblNode* GetRootNodeByName(const string& name);
    //Ensure elements of provided description exist on XtblNode. If enableOptionalSubnodes = false then optional subnodes will be created but disabled.
    void EnsureEntryExists(Handle<XtblDescription> desc, IXtblNode* node, bool enableOptionalSubnodes = true);
    //Write all nodes to xtbl file
    void WriteXtbl(const string& outPath, bool writeNanoforgeMetadata = true);
    //Propagate subnode edit state up to parents so you can check if a xtbl has any edits by checking the root nodes. Returns true if any subnode has been edited.
    bool PropagateEdits();
    //Rename category and update strings in categoryMap_
    void RenameCategory(Handle<XtblCategory> category, string newName);


    //Filename of the xtbl
    string Name;
    //Name of the vpp_pc file this xtbl is in
    string VppName;
    //Xml nodes inside the <Table></Table> block
    std::vector<IXtblNode*> Entries;
    //Xml nodes inside the <TableTemplates></TableTemplates> block
    std::vector<IXtblNode*> Templates;
    //Xml nodes inside the <TableDescription></TableDescription> block. Describes data in <Table>
    Handle<XtblDescription> TableDescription = nullptr;
    //Root category
    Handle<XtblCategory> RootCategory = nullptr;
    //References to other xtbl files
    std::vector<Handle<XtblReference>> References;
    string FilePath;

private:
    //Propagate subnode edit state up to parents so you can check if a xtbl has any edits by checking the root nodes
    bool PropagateNodeEdits(IXtblNode* node);

    //Easy way to get a nodes category with needing to search the category tree
    std::unordered_map<IXtblNode*, string> categoryMap_;

    //Xml document the xtbl was loaded from. Kept alive with the xtbl to preserve description-less nodes
    Handle<tinyxml2::XMLDocument> xmlDocument_;
};