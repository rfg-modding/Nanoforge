#pragma once
#include "gui/GuiState.h"
#include "gui/IGuiPanel.h"

class PropertyPanel : public IGuiPanel
{
public:
    PropertyPanel();
    ~PropertyPanel();

    void Update(GuiState* state, bool* open) override;

private:

};