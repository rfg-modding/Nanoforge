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


    if (table)
    {
        //Todo: Parse
        //Todo: Report warning if not present
    }
    if (templates)
    {
        //Todo: Parse if present. Optional section so don't warn if it isn't present
    }

    //Todo: Read <EntryCategories> block
    //Todo: Read <stream2_container_description> block that's present in some xtbls

    return true;
}