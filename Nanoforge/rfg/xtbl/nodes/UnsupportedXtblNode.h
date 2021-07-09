#pragma once
#include "rfg/xtbl/XtblNodes.h"
#include "Common/Typedefs.h"
#include "XtblDescription.h"
#include "IXtblNode.h"
#include "XtblType.h"
#include "imgui.h"
#include "render/imgui/imgui_ext.h"
#include "render/imgui/ImGuiConfig.h"

//Used by nodes that don't have a description in <TableDescription> so their data is preserved and Nanoforge can fully reconstruct the original xtbl if necessary
class UnsupportedXtblNode : public IXtblNode
{
public:
    friend class IXtblNode;

    UnsupportedXtblNode(tinyxml2::XMLElement* element)
    {
        element_ = element;
    }

    virtual void DrawEditor(GuiState* guiState, Handle<XtblFile> xtbl, IXtblNode* parent, const char* nameOverride = nullptr)
    {

    }

    virtual void InitDefault()
    {

    }

    virtual bool WriteModinfoEdits(tinyxml2::XMLElement* xml)
    {
        //This node type doesn't support editing
        return true;
    }

    virtual bool WriteXml(tinyxml2::XMLElement* xml, bool writeNanoforgeMetadata)
    {
        //Make a copy of element_ as a child of the document being written
        auto* copyXml = xml->InsertEndChild(element_->DeepClone(xml->GetDocument()));
        return true;
    }
private:
    tinyxml2::XMLElement* element_ = nullptr;
};