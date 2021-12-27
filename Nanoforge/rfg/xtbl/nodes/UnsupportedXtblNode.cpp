#include "XtblNodes.h"
#include <tinyxml2.h>

UnsupportedXtblNode::UnsupportedXtblNode(tinyxml2::XMLElement* element)
{
    element_ = element;
}

#pragma warning(disable:4100) //Disable warning about unused argument. Can't remove the arg since some implementations of the virtual functions use it.
bool UnsupportedXtblNode::DrawEditor(GuiState* guiState, Handle<XtblFile> xtbl, IXtblNode* parent, const char* nameOverride)
{
    return false;
}

void UnsupportedXtblNode::InitDefault()
{

}

bool UnsupportedXtblNode::WriteModinfoEdits(tinyxml2::XMLElement* xml)
{
    //This node type doesn't support editing
    return true;
}

bool UnsupportedXtblNode::WriteXml(tinyxml2::XMLElement* xml, bool writeNanoforgeMetadata)
{
    //Make a copy of element_ as a child of the document being written
    xml->InsertEndChild(element_->DeepClone(xml->GetDocument()));
    return true;
}