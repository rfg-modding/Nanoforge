#include "Xtbl.h"
#include "Log.h"
#include "common/filesystem/Path.h"
#include <tinyxml2/tinyxml2.h>
#include <filesystem>

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
        //Todo: Return bool, take XtblNode as input. Must set name and value of root nodes, must recursively call ParseTableNode for each value (e.g. <Weapon_Class>, <Animation_Group>)
        //ParseTableNode(tableElement);
        XtblNode tableNode;
        if (tableNode.Parse(tableElement))
            Entries.push_back(tableNode);

        tableElement = tableElement->NextSiblingElement(elementString.c_str());
    }


    if (templates)
    {
        //Todo: Parse if present. Optional section so don't warn if it isn't present
    }

    //Todo: Read <EntryCategories> block
    //Todo: Read <stream2_container_description> block that's present in some xtbls

    return true;
}