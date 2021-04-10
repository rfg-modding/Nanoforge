#include "Xtbl.h"
#include "Log.h"
#include "common/filesystem/Path.h"
#include "Common/string/String.h"
#include <tinyxml2/tinyxml2.h>
#include <filesystem>
#include <algorithm>
#include <iterator>

bool XtblFile::Parse(const string& path)
{
    //Ensure the file exists at the provided path
    string filename = Path::GetFileName(path);
    if (!std::filesystem::exists(path))
    {
        Log->error("Failed to parse \"{}\". File not present at that path.", path);
        return false;
    }

    //Parse the xtbl using tinyxml2
    tinyxml2::XMLDocument xtbl;
    xtbl.LoadFile(path.c_str());

    auto* root = xtbl.RootElement();
    if (!root)
    {
        Log->error("Failed to parse \"{}\". Xtbl has no <root> element.", path);
        return false;
    }

    auto* table = root->FirstChildElement("Table");
    auto* templates = root->FirstChildElement("TableTemplates");
    auto* description = root->FirstChildElement("TableDescription");

    //Parse table description
    if (!description)
    {
        Log->error("Parsing failed for {}. <TableDescription> block not found.", Path::GetFileName(path));
        return false;
    }
    if (!TableDescription->Parse(description, TableDescription))
    {
        Log->error("Parsing failed for {} <TableDescription> block.", Path::GetFileName(path));
        return false;
    }

    //Parse table
    if (!table)
    {
        Log->error("Parsing failed for {}. <Table> block not found.", Path::GetFileName(path));
        return false;
    }
    string elementString = TableDescription->Name; //Element name is the first <Name> value of the <TableDescription> section
    auto* tableElement = table->FirstChildElement(elementString.c_str());
    while (tableElement)
    {
        Handle<XtblNode> tableNode = CreateHandle<XtblNode>();
        if (tableNode->Parse(tableElement, *this, tableNode))
            Entries.push_back(tableNode);

        tableElement = tableElement->NextSiblingElement(elementString.c_str());
    }


    if (templates)
    {
        //Todo: Parse if present. Optional section so don't warn if it isn't present
    }

    //Todo: Read <EntryCategories> block
    //Todo: Read <stream2_container_description> block that's present in some xtbls

    //Add uncategorized entries to root category
    for (auto& entry : Entries)
    {
        if (!entry->CategorySet)
            RootCategory->Nodes.push_back(entry);
    }

    return true;
}

//Get description of xtbl value
Handle<XtblDescription> XtblFile::GetValueDescription(const string& valuePath, Handle<XtblDescription> desc)
{
    //Split path into components
    std::vector<s_view> split = String::SplitString(valuePath, "/");
    if (!desc)
        desc = TableDescription;

    //If path only has one component and matches current description then return this
    if (split.size() == 1 && String::EqualIgnoreCase(desc->Name, split[0]))
        return desc;

    //Otherwise loop through subnodes and try to find next component of the path
    for (auto& subnode : desc->Subnodes)
        if (String::EqualIgnoreCase(subnode->Name, split[1]))
            return GetValueDescription(valuePath.substr(valuePath.find_first_of("/") + 1), subnode);

    return nullptr;
}

void XtblFile::SetNodeCategory(Handle<XtblNode> node, s_view categoryPath)
{
    //Get category and add node to it. Takes substr that strips Entries: from the front of the path
    auto category = categoryPath.find(":") == s_view::npos ? RootCategory : GetOrCreateCategory(categoryPath.substr(categoryPath.find_first_of(":") + 1));
    category->Nodes.push_back(node);
    node->CategorySet = true;
}

Handle<XtblCategory> XtblFile::GetOrCreateCategory(s_view categoryPath, Handle<XtblCategory> parent)
{
    //Search root category if none is provided
    Handle<XtblCategory> parentCategory = parent == nullptr ? RootCategory : parent;
    auto& categories = parentCategory->SubCategories;
    
    //Split categoryPath by ':' and search for next subcategory
    Handle<XtblCategory> subcategory = nullptr;
    std::vector<s_view> split = String::SplitString(categoryPath, ":");
    auto search = std::find_if(std::begin(categories), std::end(categories), [&](Handle<XtblCategory> a) { return String::EqualIgnoreCase(a->Name, split[0]); });

    //Create next subcategory if it wasn't found
    if (search == std::end(categories))
    {
        subcategory = CreateHandle<XtblCategory>(split[0]);
        categories.push_back(subcategory);
    }
    else
    {
        subcategory = *search;
    }

    //If at end of path return current subcategory, else keep searching
    return split.size() == 1 ? subcategory : GetOrCreateCategory(categoryPath.substr(categoryPath.find_first_of(":") + 1), subcategory);
}

Handle<XtblNode> XtblFile::GetSubnode(const string& nodePath, Handle<XtblNode> searchNode)
{
    //Split path into components
    std::vector<s_view> split = String::SplitString(nodePath, "/");

    //If path only has one component and matches current description then return this
    if (split.size() == 1 && String::EqualIgnoreCase(searchNode->Name, split[0]))
        return searchNode;

    //Otherwise loop through subnodes and try to find next component of the path
    for (auto& subnode : searchNode->Subnodes)
        if (String::EqualIgnoreCase(subnode->Name, split[0]))
            return GetSubnode(nodePath.substr(nodePath.find_first_of("/") + 1), subnode);

    return nullptr;
}

std::vector<Handle<XtblNode>> XtblFile::GetSubnodes(const string& nodePath, Handle<XtblNode> searchNode, std::vector<Handle<XtblNode>>* nodes)
{
    std::vector<Handle<XtblNode>> _nodes;
    if (!nodes)
        nodes = &_nodes;

    //Split path into components
    std::vector<s_view> split = String::SplitString(nodePath, "/");

    //If path only has one component and matches current description then return this
    if (split.size() == 1 && String::EqualIgnoreCase(searchNode->Name, split[0]))
    {
        nodes->push_back(searchNode);
        return *nodes;
    }

    //Otherwise loop through subnodes and try to find next component of the path
    for (auto& subnode : searchNode->Subnodes)
        if (String::EqualIgnoreCase(subnode->Name, split[0]))
            GetSubnodes(nodePath.substr(nodePath.find_first_of("/") + 1), subnode, nodes);

    return *nodes;
}

Handle<XtblNode> XtblFile::GetRootNodeByName(const string& name)
{
    for (auto& subnode : Entries)
        if (String::EqualIgnoreCase(subnode->Name, name))
            return subnode;

    return nullptr;
}

void XtblFile::EnsureEntryExists(Handle<XtblDescription> desc, Handle<XtblNode> node)
{
    string descPath = desc->GetPath();
    descPath = descPath.substr(descPath.find_first_of('/') + 1);
    auto maybeSubnode = GetSubnode(descPath, node);
    auto subnode = maybeSubnode ? maybeSubnode : CreateHandle<XtblNode>();
    subnode->Name = desc->Name;
    subnode->Type = desc->Type;
    subnode->Parent = node;
    subnode->CategorySet = false;
    subnode->HasDescription = true;
    subnode->Enabled = true;
    switch (subnode->Type)
    {
    case XtblType::Filename:
    case XtblType::String:
        subnode->Value = "";
        break;
    case XtblType::Int:
        subnode->Value = 0;
        break;
    case XtblType::Float:
        subnode->Value = 0.0f;
        break;
    case XtblType::Vector:
        subnode->Value = Vec3(0.0f, 0.0f, 0.0f);
        break;
    case XtblType::Color:
        subnode->Value = Vec3(0.0f, 0.0f, 0.0f);
        break;
    case XtblType::Selection:
    case XtblType::Reference:
    case XtblType::Grid:
    case XtblType::ComboElement:
    case XtblType::Flags:
    case XtblType::List:
    case XtblType::Element:
    case XtblType::TableDescription:
    default:
        break;
    }
    node->Subnodes.push_back(subnode);

    for (auto& subdesc : desc->Subnodes)
        EnsureEntryExists(subdesc, subnode);
}