#include "XtblNodes.h"

#pragma warning(disable:4100) //Disable warning about unused argument. Can't remove the arg since some implementations of this function use it.
bool FlagXtblNode::DrawEditor(GuiState* guiState, Handle<XtblFile> xtbl, IXtblNode* parent, const char* nameOverride)
{
    return false;
}

void FlagXtblNode::InitDefault()
{

}

bool FlagXtblNode::WriteModinfoEdits(tinyxml2::XMLElement* xml)
{
    //Handled by FlagsXtblNode
    return true;
}

bool FlagXtblNode::WriteXml(tinyxml2::XMLElement* xml, bool writeNanoforgeMetadata)
{
    //Handled by FlagsXtblNode
    return true;
}
#pragma warning(default:4100)