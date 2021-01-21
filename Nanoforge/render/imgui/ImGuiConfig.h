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
    const ImVec4 SecondaryTextColor(0.32f, 0.67f, 1.0f, 1.00f); //Light blue;
    //const ImVec4 TertiaryTextColor(0.573f, 0.596f, 0.62f, 1.00f); //Light grey
    const ImVec4 TertiaryTextColor(0.64f, 0.67f, 0.69f, 1.00f); //Light grey
    const ImVec4 Red(0.784f, 0.094f, 0.035f, 1.0f);

    //Todo: Move into gui helpers file
    //Helpers
    static void LabelAndValue(const std::string& Label, const std::string& Value)
    {
        ImGui::Text(Label.c_str());
        ImGui::SameLine();
        ImGui::TextColored(SecondaryTextColor, Value.c_str());
    }

    /* Creates a tooltip with the given description and font on the previous ImGui element
     * created. The font argument is optional. If you leave it blank it'll use the current
     * font on the top of the stack.
     */
    static void TooltipOnPrevious(const char* Description, ImFont* Font)
    {
        if (ImGui::IsItemHovered())
        {
            if (Font)
            {
                ImGui::PushFont(Font);
            }
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
            ImGui::TextUnformatted(Description);
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
            if (Font)
            {
                ImGui::PopFont();
            }
        }
    }

    /* Creates a tooltip with the given description and font on the previous ImGui element
     * created. The font argument is optional. If you leave it blank it'll use the current
     * font on the top of the stack.
     */
    static void TooltipOnPrevious(std::string& Description, ImFont* Font)
    {
        TooltipOnPrevious(Description.c_str(), Font);
    }
}