#pragma once
#include "Common/Typedefs.h"
#include "XtblType.h"
#include "gui/GuiState.h"

class IXtblNode;
class XtblDescription;
class XtblFile;

//IXtblNode gui widget width
static f32 NodeGuiWidth = 240.0f;

//Draw a nodes editor GUI and description tooltips. They're drawn by description so non-existant optional nodes are visible.
extern void DrawNodeByDescription(GuiState* guiState, Handle<XtblFile> xtbl, Handle<XtblDescription> desc, IXtblNode* parent,
    const char* nameOverride = nullptr, IXtblNode* nodeOverride = nullptr);

//Create IXtblNode with default values for it's type/description
extern IXtblNode* CreateDefaultNode(XtblType type);
extern IXtblNode* CreateDefaultNode(Handle<XtblDescription> desc, bool addSubnodes = false);