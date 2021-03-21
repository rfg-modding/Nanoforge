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
    if (!TableDescription.Parse(description))
    {
        Log->error("Parsing failed for {} <TableDescription> block.", Path::GetFileName(path));
        return false;
    }

    //Parse table
    if (!table)
    {
        Log->error("Parsing failed for {}. <Tablen> block not found.", Path::GetFileName(path));
        return false;
    }
    string elementString = TableDescription.Name; //Element name is the first <Name> value of the <TableDescription> section
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

std::optional<XtblDescription> XtblFile::GetValueDescription(const string& valuePath)
{
    std::vector<s_view> split = String::SplitString(valuePath, "/");

    //Path should start with name of primary entry, strip this from the path
    if(String::EqualIgnoreCase(TableDescription.Name, split[0]))
        return TableDescription.GetValueDescription(valuePath.substr(valuePath.find_first_of("/") + 1));
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