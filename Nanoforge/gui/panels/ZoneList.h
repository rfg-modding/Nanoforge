#pragma once
#include "gui/IGuiPanel.h"

class GuiState;

class ZoneList : public IGuiPanel
{
public:
    ZoneList();
    ~ZoneList();

    void Update(GuiState* state, bool* open) override;

private:
    string searchTerm_ = "";
};