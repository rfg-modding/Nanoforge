#pragma once
#include "gui/IGuiPanel.h"

class GuiState;

class LogPanel : public IGuiPanel
{
public:
    LogPanel();
    ~LogPanel();

    void Update(GuiState* state, bool* open) override;

private:

};