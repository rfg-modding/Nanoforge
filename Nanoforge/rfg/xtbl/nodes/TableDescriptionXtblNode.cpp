#include "XtblNodes.h"

#pragma warning(disable:4100) //Disable warning about unused argument. Can't remove the arg since some implementations of this function use it.
bool TableDescriptionXtblNode::DrawEditor(GuiState* guiState, Handle<XtblFile> xtbl, IXtblNode* parent, const char* nameOverride)
{
    return false;
}
#pragma warning(default:4100)

void TableDescriptionXtblNode::InitDefault()
{

}

bool TableDescriptionXtblNode::WriteModinfoEdits(tinyxml2::XMLElement* xml)
{
    //Write subnodes to modinfo
    for (auto& subnode : Subnodes)
    {
        if (!subnode->Edited)
            continue;

        //Stop early if any node fails to write
        bool result = subnode->WriteModinfoEdits(xml);
        if (!result)
        {
            LOG_ERROR("Failed to write modinfo data for xtbl node \"{}\"", subnode->GetPath());
            return false;
        }
    }
    return true;
}

bool TableDescriptionXtblNode::WriteXml(tinyxml2::XMLElement* xml, bool writeNanoforgeMetadata)
{
    //Store edited state if metadata is enabled
    if (writeNanoforgeMetadata)
    {
        xml->SetAttribute("__NanoforgeEdited", Edited);
        xml->SetAttribute("__NanoforgeNewEntry", NewEntry);
    }

    //Write subnodes to xml
    for (auto& subnode : Subnodes)
    {
        //Stop early if any node fails to write
        bool result = subnode->WriteXml(xml, writeNanoforgeMetadata);
        if (!result)
        {
            LOG_ERROR("Failed to write xml data for xtbl node \"{}\"", subnode->GetPath());
            return false;
        }
    }
    return true;
}