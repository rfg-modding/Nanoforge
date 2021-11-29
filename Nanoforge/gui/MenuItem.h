#pragma once
#include "common/Typedefs.h"
#include "IGuiPanel.h"
#include <vector>

class IGuiPanel;

//Entry in the main menu bar. Items within a MenuItem are sub menus.
struct MenuItem
{
    string Text;
    std::vector<MenuItem> Items = {};
    Handle<IGuiPanel> panel = nullptr;

    MenuItem* GetItem(std::string_view text);
    void Draw();
};