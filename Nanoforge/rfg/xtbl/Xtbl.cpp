#include "Xtbl.h"
#include "Log.h"
#include "XtblNodes.h"
#include "common/filesystem/Path.h"
#include "Common/string/String.h"
#include <tinyxml2/tinyxml2.h>
#include <filesystem>
#include <algorithm>
#include <iterator>

XtblFile::~XtblFile()
{
    //Delete all nodes. Need to be manually deleted since theres circular references between parent and child nodes
    for (auto& node : Entries)
        node->DeleteSubnodes();

    Entries.clear();
}

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
    if (!TableDescription->Parse(description, TableDescription, *this))
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
        string path = tableElement->Value();
        auto node = ParseNode(tableElement, nullptr, path);
        if (node)
            Entries.push_back(node);

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

Handle<IXtblNode> XtblFile::ParseNode(tinyxml2::XMLElement* node, Handle<IXtblNode> parent, string& path)
{
    //Handle editor config nodes
    if (node->Value() == string("_Editor"))
    {
        //Set category of parent node if one is specified
        auto* category = node->FirstChildElement("Category");
        if (category)
        {
            SetNodeCategory(parent, category->GetText());
        }
        return nullptr;
    }

    //Get description and create default node of that XtblType::
    Handle<XtblDescription> desc = GetValueDescription(path);
    if (!desc)
    {
        //TODO: Report missing description somewhere so we know when description addons are needed
        return nullptr;
    }

    //Parse node from xml and set members
    Handle<IXtblNode> xtblNode = CreateDefaultNode(desc);
    xtblNode->Parent = parent;
    
    switch (xtblNode->Type)
    {
    case XtblType::String:
    {
        if (node->GetText())
            xtblNode->Value = node->GetText();
        else
            xtblNode->Value = "";
    }
    break;
    case XtblType::Int:
        xtblNode->Value = node->IntText();
        break;
    case XtblType::Float:
        xtblNode->Value = node->FloatText();
        break;
    case XtblType::Vector:
    {
        auto* x = node->FirstChildElement("X");
        auto* y = node->FirstChildElement("Y");
        auto* z = node->FirstChildElement("Z");
        Vec3 vec = { 0.0f, 0.0f, 0.0f };
        if (x)
            vec.x = x->FloatText();
        if (y)
            vec.y = y->FloatText();
        if (z)
            vec.z = z->FloatText();

        xtblNode->Value = vec;
    }
    break;
    case XtblType::Color:
    {
        auto* r = node->FirstChildElement("R");
        auto* g = node->FirstChildElement("G");
        auto* b = node->FirstChildElement("B");
        Vec3 vec = { 0.0f, 0.0f, 0.0f };
        if (r)
            vec.x = ((f32)r->IntText() / 255.0f);
        if (g)
            vec.y = ((f32)g->IntText() / 255.0f);
        if (b)
            vec.z = ((f32)b->IntText() / 255.0f);

        xtblNode->Value = vec;
    }
    break;
    case XtblType::Selection: //This is a string with restricted choices listed in TableDescription
    {
        if (node->GetText())
            xtblNode->Value = node->GetText();
        else
            xtblNode->Value = "";
    }
    break;
    case XtblType::Filename:
    {
        auto* filename = node->FirstChildElement("Filename");
        if (filename && filename->GetText())
            xtblNode->Value = filename->GetText();
        else
            xtblNode->Value = "";
    }
    break;
    case XtblType::Reference: //References data in another xtbl
    {
        if (node->GetText())
            xtblNode->Value = node->GetText();
        else
            xtblNode->Value = "";
    }
    break;
    case XtblType::Grid: //List of elements
        if (!node->FirstChildElement() && node->GetText())
        {
            xtblNode->Value = node->GetText();
        }
        else //Recursively parse subnodes if there are any
        {
            auto* subnode = node->FirstChildElement();
            while (subnode)
            {
                string newPath = path + "/" + subnode->Value();
                Handle<IXtblNode> xtblSubnode = ParseNode(subnode, xtblNode, newPath);
                if (xtblSubnode)
                    xtblNode->Subnodes.push_back(xtblSubnode);

                subnode = subnode->NextSiblingElement();
            }
        }
        break;
    case XtblType::ComboElement: //This is similar to a union in c/c++
        {
            auto* subnode = node->FirstChildElement();
            string newPath = path + "/" + subnode->Value();
            Handle<IXtblNode> xtblSubnode = ParseNode(subnode, xtblNode, newPath);
            if (xtblSubnode)
                xtblNode->Subnodes.push_back(xtblSubnode);
        }
        break;

    //These types require the subnodes to be parsed
    case XtblType::Flags: //Bitflags
    {
        //Parse flags that are present
        auto* flag = node->FirstChildElement("Flag");
        while (flag)
        {
            string newPath = path + "/" + flag->Value();
            Handle<IXtblNode> xtblSubnode = ParseNode(flag, xtblNode, newPath);
            if (xtblSubnode)
                xtblNode->Subnodes.push_back(xtblSubnode);

            flag = flag->NextSiblingElement();
        }

        //Ensure there's a node for every flag listed in <TableDescription>
        for (auto& flagDesc : desc->Flags)
        {
            //Check if flag is present
            Handle<IXtblNode> flagNode = nullptr;
            for (auto& flag : xtblNode->Subnodes)
            {
                auto value = std::get<XtblFlag>(flag->Value);
                if (value.Name == flagDesc)
                {
                    flagNode = flag;
                    break;
                }
            }

            //If flag isn't present create a subnode for it
            if (!flagNode)
            {
                auto subnode = CreateDefaultNode(XtblType::Flag);
                subnode->Name = "Flag";
                subnode->Type = XtblType::Flag;
                subnode->Parent = xtblNode;
                subnode->CategorySet = false;
                subnode->HasDescription = true;
                subnode->Enabled = true;
                subnode->Value = XtblFlag{ .Name = flagDesc, .Value = false };
                xtblNode->Subnodes.push_back(subnode);
            }
        }
    }
    break;
    case XtblType::List: //List of a single value (e.g. string, float, int)
    case XtblType::Element:
    case XtblType::TableDescription:
    default:
        if (!node->FirstChildElement() && node->GetText())
        {
            xtblNode->Value = node->GetText();
        }
        else //Recursively parse subnodes if there are any
        {
            auto* subnode = node->FirstChildElement();
            while (subnode)
            {
                string newPath = path + "/" + subnode->Value();
                Handle<IXtblNode> xtblSubnode = ParseNode(subnode, xtblNode, newPath);
                if (xtblSubnode)
                    xtblNode->Subnodes.push_back(xtblSubnode);

                subnode = subnode->NextSiblingElement();
            }
        }
        break;
    }

    return xtblNode;
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

void XtblFile::SetNodeCategory(Handle<IXtblNode> node, s_view categoryPath)
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

Handle<IXtblNode> XtblFile::GetSubnode(const string& nodePath, Handle<IXtblNode> searchNode)
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

std::vector<Handle<IXtblNode>> XtblFile::GetSubnodes(const string& nodePath, Handle<IXtblNode> searchNode, std::vector<Handle<IXtblNode>>* nodes)
{
    std::vector<Handle<IXtblNode>> _nodes;
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

Handle<IXtblNode> XtblFile::GetRootNodeByName(const string& name)
{
    for (auto& subnode : Entries)
        if (String::EqualIgnoreCase(subnode->Name, name))
            return subnode;

    return nullptr;
}

void XtblFile::EnsureEntryExists(Handle<XtblDescription> desc, Handle<IXtblNode> node)
{
    //Try to get pre-existing subnode
    string descPath = desc->GetPath();
    descPath = descPath.substr(descPath.find_first_of('/') + 1);
    auto maybeSubnode = GetSubnode(descPath, node);
    bool subnodeExists = maybeSubnode != nullptr;

    //Get subnode or create default one if it doesn't exist
    auto subnode = subnodeExists ? maybeSubnode : CreateDefaultNode(desc);
    if (!subnodeExists)
    {
        subnode->Name = desc->Name;
        subnode->Type = desc->Type;
        subnode->Parent = node;
        subnode->CategorySet = false;
        subnode->HasDescription = true;
        subnode->Enabled = true;
        node->Subnodes.push_back(subnode);
    }

    for (auto& subdesc : desc->Subnodes)
        EnsureEntryExists(subdesc, subnode);
}