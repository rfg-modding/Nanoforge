#pragma once
#include "common/string/String.h"
#include "common/Handle.h"
#include "documents/IDocument.h"
#include "RfgTools++/types/Vec3.h"

class DX11Renderer;
class PackfileVFS;
class Territory;
class GuiState;
class Project;
class Config;
class Localization;
class TextureIndex;
class ZoneObjectNode36;
class ZoneObject36;
class XtblManager;
class ImGuiFontManager;
struct FileExplorerNode;
//Function signature for property panel content functions. Swapping these out lets you easily change what it's displaying info for
using PropertyPanelContentFunc = void(GuiState* state);

//Used to color status bar
enum GuiStatus
{
    Ready, //Used when the app isn't doing any background work and the gui / overlays are completely ready for use
    Working, //Used when the app is doing background work. User may have to wait for the work to complete to use the entire gui
    Error, //Used when a serious error that requires user attention is encountered
    None //Used for default function arguments
};

class GuiState
{
public:
    ImGuiFontManager* FontManager = nullptr;
    PackfileVFS* PackfileVFS = nullptr;
    DX11Renderer* Renderer = nullptr;
    Project* CurrentProject = nullptr;
    XtblManager* Xtbls = nullptr;
    Localization* Localization = nullptr;
    TextureIndex* TextureSearchIndex = nullptr;

    //Most recently selected territory. If you have multiple territories open this is the most recently selected window
    Territory* CurrentTerritory = nullptr;
    bool CurrentTerritoryUpdateDebugDraw = true;

    //Todo: This would be better handled via an event system
    //Used to trigger camera position changes in the focused territory
    bool CurrentTerritoryCamPosNeedsUpdate = false;
    Vec3 CurrentTerritoryNewCamPos;

    GuiStatus Status = Ready;
    string CustomStatusMessage = "";
    f32 StatusBarHeight = 25.0f;

    ZoneObjectNode36* SelectedObject = nullptr;

    //Used to trigger and reload and load a different territory
    string CurrentTerritoryName = "terr01";
    string CurrentTerritoryShortname = "zonescript_terr01.vpp_pc";

    ZoneObject36* ZoneObjectList_SelectedObject = nullptr;

    //Content func for property panel
    PropertyPanelContentFunc* PropertyPanelContentFuncPtr = nullptr;

    //Documents that are currently open
    std::vector<Handle<IDocument>> Documents = {};
    //Documents that were created this frame.
    //Moved to main vector next frame so creating a document during gui update doesn't invalidate the iterator at MainGui::Update()
    std::vector<Handle<IDocument>> NewDocuments = {};

    //The last node that was clicked in the file explorer
    FileExplorerNode* FileExplorer_SelectedNode = nullptr;

    //Set status message and enum
    void SetStatus(std::string_view message, GuiStatus status = None)
    {
        //Set status enum too if it's not the default argument value
        if (status != None)
            Status = status;

        CustomStatusMessage = message;
    }

    //Clear status message and set status enum to 'Ready'
    void ClearStatus()
    {
        Status = Ready;
        CustomStatusMessage = "";
    }

    void SetSelectedZoneObject(ZoneObjectNode36* object)
    {
        SelectedObject = object;
    }

    void SetTerritory(const string& newTerritory)
    {
        string terr = newTerritory;
        if (terr == "terr01")
            terr = "zonescript_terr01";
        if (terr == "dlc01")
            terr = "zonescript_dlc01";

        terr += ".vpp_pc";

        CurrentTerritoryName = terr;
        CurrentTerritoryShortname = newTerritory;
    }

    //Create and init a document
    void CreateDocument(string title, Handle<IDocument> document)
    {
        //Make sure document with same title doesn't already exist
        for (auto& doc : Documents)
            if (String::EqualIgnoreCase(doc->Title, title))
                return;
        for (auto& doc : NewDocuments)
            if (String::EqualIgnoreCase(doc->Title, title))
                return;

        //Add document to list
        document->Title = title;
        NewDocuments.emplace_back(document);
    }

    //Get document by title. Returns nullptr if document doesn't exist
    Handle<IDocument> GetDocument(string& title)
    {
        //Get document if it exists.
        for (auto& doc : Documents)
            if (String::EqualIgnoreCase(doc->Title, title))
                return doc;

        //Return nullptr if it doesn't exist
        return nullptr;
    }
};