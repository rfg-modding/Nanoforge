#pragma once
#include "common/Typedefs.h"
#include <imgui.h>
#include <IconsFontAwesome5_c.h>

namespace gui
{
    static const char* FontPath = "assets/fonts/Ruda-Bold.ttf";
    static const char* FontAwesomeFontPath = "assets/fonts/fa-solid-900.ttf";
}

namespace gui
{
    const ImVec4 SecondaryTextColor(0.2f, 0.7f, 1.0f, 1.00f); //Light blue;
    const ImVec4 Red(0.784f, 0.094f, 0.035f, 1.0f);

    //Todo: Move into gui helpers file
    //Helpers
    static void LabelAndValue(const std::string& Label, const std::string& Value)
    {
        ImGui::Text(Label.c_str());
        ImGui::SameLine();
        ImGui::TextColored(SecondaryTextColor, Value.c_str());
    }
}