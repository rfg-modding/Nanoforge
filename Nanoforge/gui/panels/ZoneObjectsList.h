#pragma once
#include "gui/IGuiPanel.h"

class GuiState;
struct ZoneData;
class ZoneObjectNode36;

class ZoneObjectsList : public IGuiPanel
{
public:
    ZoneObjectsList();
    ~ZoneObjectsList();

    void Update(GuiState* state, bool* open) override;

private:
    //Draw tree node for zone object and recursively draw child objects
    void DrawObjectNode(GuiState* state, ZoneObjectNode36& object);
    //Returns true if any objects in the zone are visible
    bool ZoneAnyChildObjectsVisible(GuiState* state, ZoneData& zone);
    //Returns true if the object or any of it's children are visible
    bool ShowObjectOrChildren(GuiState* state, ZoneObjectNode36& object);

    string searchTerm_ = "";
    //If true zones outside the territory viewing distance are hidden. Configurable via the buttons in the top left of the territory viewer.
    bool onlyShowNearZones_ = true;
};