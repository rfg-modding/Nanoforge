#pragma once
#include "common/Typedefs.h"
#include "IGuiPanel.h"
#include <vector>

struct MenuItem
{
    string Text;
    std::vector<MenuItem> Items = {};
    //Todo: Must update if we resize MainGui::panels_. Currently do not do this
    Handle<IGuiPanel> panel = nullptr;

    MenuItem* GetItem(const string& text)
    {
        for (auto& item : Items)
        {
            if (item.Text == text)
                return &item;
        }
        return nullptr;
    }
    void Draw()
    {
        if (panel)
        {
            ImGui::MenuItem(Text.c_str(), "", &panel->Open);
            return;
        }

        if (ImGui::BeginMenu(Text.c_str()))
        {
            for (auto& item : Items)
            {
                item.Draw();
            }
            ImGui::EndMenu();
        }
    }
};