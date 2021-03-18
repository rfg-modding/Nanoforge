#pragma once
#include "gui/GuiState.h"
#include "gui/IGuiPanel.h"

class ZoneList : public IGuiPanel
{
public:
    ZoneList();
    ~ZoneList();

    void Update(GuiState* state, bool* open) override;

private:
    string searchTerm_ = "";
};