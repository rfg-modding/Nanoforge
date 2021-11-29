#pragma once
#include "gui/IGuiPanel.h"

class GuiState;

class PropertyPanel : public IGuiPanel
{
public:
    PropertyPanel();
    ~PropertyPanel();

    void Update(GuiState* state, bool* open) override;

private:

};