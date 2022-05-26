#pragma once
#include "gui/IGuiPanel.h"
#include "application/Registry.h"

class GuiState;

class ZoneObjectsList : public IGuiPanel
{
public:
    ZoneObjectsList();
    ~ZoneObjectsList();

    void Update(GuiState* state, bool* open) override;

private:
    void DrawFilters(GuiState* state);
    //Draw tree node for zone object and recursively draw child objects
    void DrawObjectNode(GuiState* state, ObjectHandle object);
    //Returns true if any objects in the zone are visible
    bool ZoneAnyChildObjectsVisible(GuiState* state, ObjectHandle zone);
    //Returns true if the object or any of it's children are visible
    bool ShowObjectOrChildren(GuiState* state, ObjectHandle object);

    string searchTerm_ = "";
    //If true zones outside the territory viewing distance are hidden. Configurable via the buttons in the top left of the territory viewer.
    bool onlyShowNearZones_ = true;
    bool onlyShowPersistentObjects_ = false;
};