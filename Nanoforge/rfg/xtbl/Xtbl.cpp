#include "Xtbl.h"
#include "Log.h"
#include "nodes/XtblNodes.h"
#include "common/filesystem/Path.h"
#include "Common/string/String.h"
#include "IXtblNode.h"
#include "rfg/PackfileVFS.h"
#include <tinyxml2/tinyxml2.h>
#include <filesystem>
#include <algorithm>
#include <iterator>

XtblFile::~XtblFile()
{
    //Clear category map
    categoryMap_.clear();

    //Delete all nodes. Need to be manually deleted since theres circular references between parent and child nodes
    for (auto& node : Entries)
        delete node;

    Entries.clear();
}

bool XtblFile::Reload(PackfileVFS* packfileVFS)
{
    for (auto entry : Entries)
        delete entry;
    for (auto templ : Templates)
        delete templ;

    Entries.clear();
    Templates.clear();
    References.clear();
    TableDescription = CreateHandle<XtblDescription>();
    RootCategory = CreateHandle<XtblCategory>("Entries");

    return Parse(FilePath, packfileVFS);
}

bool XtblFile::Parse(const string& path, PackfileVFS* packfileVFS)
{
    //Read file into a string
    FilePath = path;
    std::optional<string> fileString = packfileVFS->GetFileString(path);
    if (!fileString)
    {
        LOG_ERROR("Failed to read \"{}\" from packfile.", path);
        return false;
    }

    //Parse the xtbl using tinyxml2
    xmlDocument_.Parse(fileString.value().data(), fileString.value().size());

    auto* root = xmlDocument_.RootElement();
    if (!root)
    {
        LOG_ERROR("Failed to parse \"{}\". Xtbl has no <root> element.", path);
        return false;
    }

    auto* table = root->FirstChildElement("Table");
    auto* templates = root->FirstChildElement("TableTemplates");
    auto* description = root->FirstChildElement("TableDescription");

    //Parse table description
    if (!description)
    {
        LOG_ERROR("Parsing failed for {}. <TableDescription> block not found.", Path::GetFileName(path));
        return false;
    }
    if (!TableDescription->Parse(description, TableDescription, *this))
    {
        LOG_ERROR("Parsing failed for {} <TableDescription> block.", Path::GetFileName(path));
        return false;
    }

    //Parse table
    if (!table)
    {
        LOG_ERROR("Parsing failed for {}. <Table> block not found.", Path::GetFileName(path));
        return false;
    }
    string elementString = TableDescription->Name; //Element name is the first <Name> value of the <TableDescription> section
    auto* tableElement = table->FirstChildElement(elementString.c_str());
    while (tableElement)
    {
        string nodePath = tableElement->Value();
        auto node = ParseNode(tableElement, nullptr, nodePath);
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

IXtblNode* XtblFile::ParseNode(tinyxml2::XMLElement* node, IXtblNode* parent, string& path)
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
        //TODO: Report missing description somewhere in release builds so we know when description addons are needed
        //Create UnsupportedXtblNode to preserve xml data
        UnsupportedXtblNode* xtblNode = new UnsupportedXtblNode(node);
        xtblNode->Name = node->Value();
        xtblNode->Type = XtblType::Unsupported;
        xtblNode->Parent = parent;
        xtblNode->Edited = false;
        return xtblNode;
    }

    //Parse node from xml and set members
    IXtblNode* xtblNode = CreateDefaultNode(desc, false);
    xtblNode->Parent = parent;

    //Read nanoforge metadata if it's present
    auto* editedAttribute = node->FindAttribute("__NanoforgeEdited");
    auto* newEntryAttribute = node->FindAttribute("__NanoforgeNewEntry");
    xtblNode->Edited = editedAttribute ? editedAttribute->BoolValue() : false;
    xtblNode->NewEntry = newEntryAttribute ? newEntryAttribute->BoolValue() : false;

    //Read node data
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
                IXtblNode* xtblSubnode = ParseNode(subnode, xtblNode, newPath);
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
            IXtblNode* xtblSubnode = ParseNode(subnode, xtblNode, newPath);
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
            IXtblNode* xtblSubnode = CreateDefaultNode(XtblType::Flag);
            xtblSubnode->Name = "Flag";
            xtblSubnode->Type = XtblType::Flag;
            xtblSubnode->Parent = xtblNode;
            xtblSubnode->CategorySet = false;
            xtblSubnode->HasDescription = true;
            xtblSubnode->Enabled = true;
            xtblSubnode->Value = XtblFlag{ .Name = flag->GetText(), .Value = true };
            xtblNode->Subnodes.push_back(xtblSubnode);

            flag = flag->NextSiblingElement();
        }

        //Ensure there's a node for every flag listed in <TableDescription>
        for (auto& flagDesc : desc->Flags)
        {
            //Check if flag is present
            IXtblNode* flagNode = nullptr;
            for (auto& subnode : xtblNode->Subnodes)
            {
                XtblFlag& value = std::get<XtblFlag>(subnode->Value);
                if (value.Name == flagDesc)
                {
                    flagNode = subnode;
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
                IXtblNode* xtblSubnode = ParseNode(subnode, xtblNode, newPath);
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

void XtblFile::SetNodeCategory(IXtblNode* node, string categoryPath)
{
    //Ensure path starts with "Entries:" so it's parsed properly. Some xtbls do this and some don't.
    if (!String::StartsWith(categoryPath, "Entries"))
        categoryPath.insert(0, "Entries:");
    //Strip extraneous colon. Colon should always be followed by more subcategories. Otherwise would break other code that parses categories.
    if (categoryPath.back() == ':')
        categoryPath = { categoryPath.data(), categoryPath.size() - 1 };

    //Get category and add node to it. Takes substr that strips Entries: from the front of the path
    auto category = categoryPath.find(":") == string::npos ? RootCategory : GetOrCreateCategory(categoryPath.substr(categoryPath.find_first_of(":") + 1));
    category->Nodes.push_back(node);
    node->CategorySet = true;
    categoryMap_[node] = categoryPath;
}

string XtblFile::GetNodeCategoryPath(IXtblNode* node)
{
    auto find = categoryMap_.find(node);
    return find == categoryMap_.end() ? "" : find->second;
}

Handle<XtblCategory> XtblFile::GetNodeCategory(IXtblNode* node)
{
    auto find = categoryMap_.find(node);
    return find == categoryMap_.end() ? nullptr : GetOrCreateCategory(find->second);
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

IXtblNode* XtblFile::GetSubnode(const string& nodePath, IXtblNode* searchNode)
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

std::vector<IXtblNode*> XtblFile::GetSubnodes(const string& nodePath, IXtblNode* searchNode, std::vector<IXtblNode*>* nodes)
{
    std::vector<IXtblNode*> _nodes;
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

IXtblNode* XtblFile::GetRootNodeByName(const string& name)
{
    for (auto& subnode : Entries)
        if (String::EqualIgnoreCase(subnode->Name, name))
            return subnode;

    return nullptr;
}

void XtblFile::EnsureEntryExists(Handle<XtblDescription> desc, IXtblNode* node, bool enableOptionalSubnodes)
{
    //Try to get pre-existing subnode
    string descPath = desc->GetPath();
    descPath = descPath.substr(descPath.find_last_of('/') + 1);
    auto split = String::SplitString(descPath, "/");
    auto maybeSubnode = GetSubnode(descPath, node);
    bool subnodeExists = maybeSubnode != nullptr;

    //Get parent node. Might be several levels deep
    auto parent = split.size() == 1 ? node : GetSubnode(string(split[0]), node);

    //Get subnode or create default one if it doesn't exist
    auto subnode = subnodeExists ? maybeSubnode : CreateDefaultNode(desc, false);
    if (!subnodeExists)
    {
        subnode->Name = desc->Name;
        subnode->Type = desc->Type;
        subnode->Parent = parent;
        subnode->CategorySet = false;
        subnode->HasDescription = true;
        subnode->Enabled = desc->Required || enableOptionalSubnodes;
        parent->Subnodes.push_back(subnode);
    }

    //Make sure all Flags are present for Flags nodes. Missing ones are added but disabled.
    if (subnode->Type == XtblType::Flags)
    {
        //Ensure there's a subnode for every flag listed in <TableDescription>
        for (auto& flagDesc : desc->Flags)
        {
            //Check for flag
            IXtblNode* flagNode = nullptr;
            for (auto& flag : subnode->Subnodes)
            {
                XtblFlag& value = std::get<XtblFlag>(flag->Value);
                if (value.Name == flagDesc)
                {
                    flagNode = flag;
                    break;
                }
            }

            //If flag isn't present create a subnode for it
            if (!flagNode)
            {
                flagNode = CreateDefaultNode(XtblType::Flag);
                flagNode->Name = "Flag";
                flagNode->Type = XtblType::Flag;
                flagNode->Parent = subnode;
                flagNode->CategorySet = false;
                flagNode->HasDescription = true;
                flagNode->Enabled = true;
                flagNode->Value = XtblFlag{ .Name = flagDesc, .Value = false };
                subnode->Subnodes.push_back(flagNode);
            }
        }
    }

    //Make sure all subnodes are present
    for (auto& subdesc : desc->Subnodes)
        EnsureEntryExists(subdesc, subnode, enableOptionalSubnodes);
}

void XtblFile::WriteXtbl(const string& outPath, bool writeNanoforgeMetadata)
{
    //Create XmlDocument, root node, and main sections of xtbl
    tinyxml2::XMLDocument document;
    auto* root = document.NewElement("root");
    document.InsertFirstChild(root);
    auto* table = root->InsertNewChildElement("Table");
    //auto* templates = root->InsertNewChildElement("TableTemplates");
    auto* description = root->InsertNewChildElement("TableDescription");

    //Write entry data
    for (auto& entry : Entries)
    {
        auto* entryXml = table->InsertNewChildElement(entry->Name.c_str());
        entry->WriteXml(entryXml, writeNanoforgeMetadata);

        //Write category if entry has one
        string category = GetNodeCategoryPath(entry);
        if (category != "")
        {
            auto* editorXml = entryXml->InsertNewChildElement("_Editor");
            auto* categoryXml = editorXml->InsertNewChildElement("Category");
            categoryXml->SetText(category.c_str());
        }
    }

    //Todo: Implement
    //Write templates

    //Write descriptions
    TableDescription->WriteXml(description);

    //Write xml to output path
    document.SaveFile(outPath.c_str());
}

bool XtblFile::PropagateEdits()
{
    //Run on all subnodes and return true if any subnode has been edited
    bool anySubnodeEdited = false;
    for (auto& subnode : Entries)
        if (PropagateNodeEdits(subnode))
            anySubnodeEdited = true;

    //No subnode has been edited, return false
    return anySubnodeEdited;
}

void XtblFile::RenameCategory(Handle<XtblCategory> category, string newName)
{
    //Update node name
    string oldName = category->Name;
    category->Name = newName;

    //Update node name strings in categoryMap_
    for (auto& pair : categoryMap_)
        if (String::Contains(pair.second, oldName))
            pair.second = String::Replace(pair.second, oldName, newName);
}

bool XtblFile::PropagateNodeEdits(IXtblNode* node)
{
    //If node is edited mark all of it's parents as edited
    if (node->Edited)
    {
        IXtblNode* parent = node->Parent;
        while (parent)
        {
            parent->Edited = true;
            parent = parent->Parent;
        }
    }

    //Check if subnodes have been edited
    bool anySubnodeEdited = false;
    for (auto& subnode : node->Subnodes)
    {
        if (PropagateNodeEdits(subnode))
        {
            anySubnodeEdited = true;

            //Special case for list variables nodes. Set name nodes as edited so they're written to the modinfo.
            //They're needed to differentiate each list entry from each other
            if (node->Type == XtblType::List)
            {
                auto nameNode = subnode->GetSubnode("Name");
                if (nameNode)
                    nameNode->Edited = true;
            }
        }
    }

    //Return true if this node or any of it's subnodes were edited
    return anySubnodeEdited || node->Edited;
}
