#pragma once
#include "Common/Typedefs.h"
#include "rfg/xtbl/IXtblNode.h"
#include "rfg/xtbl/XtblType.h"
#include "RfgTools++/types/Vec2.h"
#include "rfg/Localization.h"

class XtblDescription;
class XtblFile;
class GuiState;
namespace tinyxml2
{
    class XMLElement;
}

//IXtblNode gui widget width
static f32 NodeGuiWidth = 240.0f;

//Draw a nodes editor GUI and description tooltips. They're drawn by description so non-existant optional nodes are visible.
extern bool DrawNodeByDescription(GuiState* guiState, Handle<XtblFile> xtbl, Handle<XtblDescription> desc, IXtblNode* parent,
    const char* nameOverride = nullptr, IXtblNode* nodeOverride = nullptr);

//Create IXtblNode with default values for it's type/description
extern IXtblNode* CreateDefaultNode(XtblType type);
extern IXtblNode* CreateDefaultNode(Handle<XtblDescription> desc, bool addSubnodes = false);


/*IXtblNode classes*/

//Node with a RGB color value
class ColorXtblNode : public IXtblNode
{
public:
    virtual bool DrawEditor(GuiState* guiState, Handle<XtblFile> xtbl, IXtblNode* parent, const char* nameOverride = nullptr);
    virtual void InitDefault();
    virtual bool WriteModinfoEdits(tinyxml2::XMLElement* xml);
    virtual bool WriteXml(tinyxml2::XMLElement* xml, bool writeNanoforgeMetadata);
};

//Node that can have multiple value types. Similar to a union in C/C++
class ComboElementXtblNode : public IXtblNode
{
public:
    virtual bool DrawEditor(GuiState* guiState, Handle<XtblFile> xtbl, IXtblNode* parent, const char* nameOverride = nullptr);
    virtual void InitDefault();
    virtual bool WriteModinfoEdits(tinyxml2::XMLElement* xml);
    virtual bool WriteXml(tinyxml2::XMLElement* xml, bool writeNanoforgeMetadata);
};

//A node that contains many other nodes. Usually the root node of an stringXml is an element node. E.g. each <Vehicle> block is vehicles.xtbl is an element node.
class ElementXtblNode : public IXtblNode
{
public:
    virtual bool DrawEditor(GuiState* guiState, Handle<XtblFile> xtbl, IXtblNode* parent, const char* nameOverride = nullptr);
    virtual void InitDefault();
    virtual bool WriteModinfoEdits(tinyxml2::XMLElement* xml);
    virtual bool WriteXml(tinyxml2::XMLElement* xml, bool writeNanoforgeMetadata);
};

//Node with the name of an RFG asset file
class FilenameXtblNode : public IXtblNode
{
public:
    virtual bool DrawEditor(GuiState* guiState, Handle<XtblFile> xtbl, IXtblNode* parent, const char* nameOverride = nullptr);
    virtual void InitDefault();
    virtual bool WriteModinfoEdits(tinyxml2::XMLElement* xml);
    virtual bool WriteXml(tinyxml2::XMLElement* xml, bool writeNanoforgeMetadata);
};

//Node with set of flags that can be true or false. Flags are described in xtbl description block
class FlagsXtblNode : public IXtblNode
{
public:
    virtual bool DrawEditor(GuiState* guiState, Handle<XtblFile> xtbl, IXtblNode* parent, const char* nameOverride = nullptr);
    virtual void InitDefault();
    virtual bool WriteModinfoEdits(tinyxml2::XMLElement* xml);
    virtual bool WriteXml(tinyxml2::XMLElement* xml, bool writeNanoforgeMetadata);
};

//Flag node. Contained by FlagsXtblNode
class FlagXtblNode : public IXtblNode
{
public:
    virtual bool DrawEditor(GuiState* guiState, Handle<XtblFile> xtbl, IXtblNode* parent, const char* nameOverride = nullptr);
    virtual void InitDefault();
    virtual bool WriteModinfoEdits(tinyxml2::XMLElement* xml);
    virtual bool WriteXml(tinyxml2::XMLElement* xml, bool writeNanoforgeMetadata);
};

//Node with a floating point value
class FloatXtblNode : public IXtblNode
{
public:
    virtual bool DrawEditor(GuiState* guiState, Handle<XtblFile> xtbl, IXtblNode* parent, const char* nameOverride = nullptr);
    virtual void InitDefault();
    virtual bool WriteModinfoEdits(tinyxml2::XMLElement* xml);
    virtual bool WriteXml(tinyxml2::XMLElement* xml, bool writeNanoforgeMetadata);
};

//Node which is a table of values with one or more rows and columns
class GridXtblNode : public IXtblNode
{
public:
    virtual bool DrawEditor(GuiState* guiState, Handle<XtblFile> xtbl, IXtblNode* parent, const char* nameOverride = nullptr);
    virtual void InitDefault();
    virtual bool WriteModinfoEdits(tinyxml2::XMLElement* xml);
    virtual bool WriteXml(tinyxml2::XMLElement* xml, bool writeNanoforgeMetadata);
};

//Node with a signed integer value
class IntXtblNode : public IXtblNode
{
public:
    virtual bool DrawEditor(GuiState* guiState, Handle<XtblFile> xtbl, IXtblNode* parent, const char* nameOverride = nullptr);
    virtual void InitDefault();
    virtual bool WriteModinfoEdits(tinyxml2::XMLElement* xml);
    virtual bool WriteXml(tinyxml2::XMLElement* xml, bool writeNanoforgeMetadata);
};

//Node containing a list of other nodes
class ListXtblNode : public IXtblNode
{
public:
    virtual bool DrawEditor(GuiState* guiState, Handle<XtblFile> xtbl, IXtblNode* parent, const char* nameOverride = nullptr);
    virtual void InitDefault();
    void MarkAsEditedRecursive(IXtblNode* node);
    virtual bool WriteModinfoEdits(tinyxml2::XMLElement* xml);
    virtual bool WriteXml(tinyxml2::XMLElement* xml, bool writeNanoforgeMetadata);
};

//Node which references the value of nodes in another xtbl
class ReferenceXtblNode : public IXtblNode
{
public:
    void CollectReferencedNodes(GuiState* guiState, Handle<XtblFile> xtbl);
    virtual bool DrawEditor(GuiState* guiState, Handle<XtblFile> xtbl, IXtblNode* parent, const char* nameOverride = nullptr);
    virtual void InitDefault();
    virtual bool WriteModinfoEdits(tinyxml2::XMLElement* xml);
    virtual bool WriteXml(tinyxml2::XMLElement* xml, bool writeNanoforgeMetadata);

private:
    //Search string used in reference list
    string searchTerm_ = "";

    //Cached list of nodes being referenced
    bool collectedReferencedNodes_ = false;
    Handle<XtblFile> refXtbl_ = nullptr;
    std::vector<IXtblNode*> referencedNodes_ = {}; //Referenced nodes in another xtbl

    //Values used to align reference with buttons that jump to their xtbl
    Vec2 maxReferenceSize_ = { 0.0f, 0.0f };
    const f32 comboButtonWidth_ = 26.5f;
    const f32 comboButtonHeight_ = 25.0f;
    const f32 comboButtonOffsetY_ = -5.0f;
};

//Node with preset options described in xtbl description block
class SelectionXtblNode : public IXtblNode
{
public:
    virtual bool DrawEditor(GuiState* guiState, Handle<XtblFile> xtbl, IXtblNode* parent, const char* nameOverride = nullptr);
    virtual void InitDefault();
    virtual bool WriteModinfoEdits(tinyxml2::XMLElement* xml);
    virtual bool WriteXml(tinyxml2::XMLElement* xml, bool writeNanoforgeMetadata);

private:
    string searchTerm_ = "";
};

//Node with a string value
class StringXtblNode : public IXtblNode
{
public:
    virtual bool DrawEditor(GuiState* guiState, Handle<XtblFile> xtbl, IXtblNode* parent, const char* nameOverride = nullptr);
    virtual void InitDefault();
    virtual bool WriteModinfoEdits(tinyxml2::XMLElement* xml);
    virtual bool WriteXml(tinyxml2::XMLElement* xml, bool writeNanoforgeMetadata);

private:
    void CheckForLocalization(GuiState* guiState);

    Locale locale_ = Locale::None;
    std::optional<string> localizedString_ = {};
};

//Node found in xtbl description block found at the end of xtbl files
class TableDescriptionXtblNode : public IXtblNode
{
public:
    virtual bool DrawEditor(GuiState* guiState, Handle<XtblFile> xtbl, IXtblNode* parent, const char* nameOverride = nullptr);
    virtual void InitDefault();
    virtual bool WriteModinfoEdits(tinyxml2::XMLElement* xml);
    virtual bool WriteXml(tinyxml2::XMLElement* xml, bool writeNanoforgeMetadata);
};

//Used by nodes that don't have a description in <TableDescription> so their data is preserved and Nanoforge can fully reconstruct the original xtbl if necessary
class UnsupportedXtblNode : public IXtblNode
{
public:
    friend class IXtblNode;
    UnsupportedXtblNode(tinyxml2::XMLElement* element);
    virtual bool DrawEditor(GuiState* guiState, Handle<XtblFile> xtbl, IXtblNode* parent, const char* nameOverride = nullptr);
    virtual void InitDefault();
    virtual bool WriteModinfoEdits(tinyxml2::XMLElement* xml);
    virtual bool WriteXml(tinyxml2::XMLElement* xml, bool writeNanoforgeMetadata);

private:
    tinyxml2::XMLElement* element_ = nullptr;
};

//Node with a 3d vector value
class VectorXtblNode : public IXtblNode
{
public:
    virtual bool DrawEditor(GuiState* guiState, Handle<XtblFile> xtbl, IXtblNode* parent, const char* nameOverride = nullptr);
    virtual void InitDefault();
    virtual bool WriteModinfoEdits(tinyxml2::XMLElement* xml);
    virtual bool WriteXml(tinyxml2::XMLElement* xml, bool writeNanoforgeMetadata);
};