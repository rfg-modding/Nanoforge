#include "MenuItem.h"
#include "IGuiPanel.h"
#include <string_view>
#include <imgui.h>

MenuItem* MenuItem::GetItem(std::string_view text)
{
    for (auto& item : Items)
    {
        if (item.Text == text)
            return &item;
    }
    return nullptr;
}

void MenuItem::Draw()
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