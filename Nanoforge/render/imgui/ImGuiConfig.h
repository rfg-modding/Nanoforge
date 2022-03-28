#pragma once
#include "common/Typedefs.h"
#include "common/String.h"
#include <IconsFontAwesome5_c.h>
#include <imgui.h>

//Used by gui::SetThemePreset()
enum ThemePreset
{
    Dark,
    Orange,
    Blue
};

namespace gui
{
    //See ImGuiConfig.cpp for values
    extern ImVec4 SecondaryTextColor;
    extern ImVec4 TertiaryTextColor;
    extern ImVec4 Red;

#pragma warning(disable:4505)
    //Set formatted to false to disable imgui printf style formatting. E.g. displaying RFG localization strings without substituting values that the game replaces at runtime.
    static void LabelAndValue(const std::string& Label, const std::string& Value, bool formatted = true)
    {
        if (formatted)
        {
            ImGui::Text(Label.c_str());
            ImGui::SameLine();
            ImGui::TextColored(SecondaryTextColor, Value.c_str());
        }
        else
        {
            ImGui::TextUnformatted(Label.c_str());
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, SecondaryTextColor);
            ImGui::TextUnformatted(Value.c_str());
            ImGui::PopStyleColor();
        }
    }

    /* Creates a tooltip with the given description and font on the previous ImGui element
     * created. The font argument is optional. If you leave it blank it'll use the current
     * font on the top of the stack.
     */
    static void TooltipOnPrevious(const char* Description, ImFont* Font = nullptr)
    {
        if (Font == nullptr)
            Font = ImGui::GetFont();

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

    //Draws (?) with tooltip
    static void HelpMarker(const string& tooltip, ImFont* font)
    {
        ImGui::TextDisabled("(?)");
        TooltipOnPrevious(tooltip.c_str(), font);
    }

    /* Creates a tooltip with the given description and font on the previous ImGui element
     * created. The font argument is optional. If you leave it blank it'll use the current
     * font on the top of the stack.
     */
    static void TooltipOnPrevious(const std::string& Description, ImFont* Font)
    {
        TooltipOnPrevious(Description.c_str(), Font);
    }

    //Set imgui theme
    static void SetThemePreset(ThemePreset preset)
    {
        ImGuiStyle* style = &ImGui::GetStyle();
        ImVec4* colors = style->Colors;

        switch (preset)
        {
        case Dark:
            //Start with imgui dark theme
            SecondaryTextColor = { 0.32f, 0.67f, 1.0f, 1.00f }; //Light blue
            ImGui::StyleColorsDark();
            style->WindowPadding = { 8, 8 };
            style->FramePadding = { 5, 5 };
            style->ItemSpacing = { 8, 8 };
            style->ItemInnerSpacing = { 8, 6 };
            style->IndentSpacing = 25.0f;
            style->ScrollbarSize = 18.0f;
            style->GrabMinSize = 12.0f;
            style->WindowBorderSize = 0.0f;
            style->ChildBorderSize = 1.0f;
            style->PopupBorderSize = 1.0f;
            style->FrameBorderSize = 1.0f;
            style->TabBorderSize = 0.0f;
            style->WindowRounding = 4.0f;
            style->ChildRounding = 0.0f;
            style->FrameRounding = 4.0f;
            style->PopupRounding = 4.0f;
            style->ScrollbarRounding = 4.0f;
            style->GrabRounding = 4.0f;
            style->TabRounding = 0.0f;

            colors[ImGuiCol_Text] = ImVec4(0.96f, 0.96f, 0.99f, 1.00f);
            colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
            colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
            colors[ImGuiCol_ChildBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
            colors[ImGuiCol_PopupBg] = ImVec4(0.09f, 0.09f, 0.10f, 1.00f);
            colors[ImGuiCol_Border] = ImVec4(0.216f, 0.216f, 0.216f, 1.0f);
            colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
            colors[ImGuiCol_FrameBg] = ImVec4(0.161f, 0.161f, 0.176f, 1.00f);
            colors[ImGuiCol_FrameBgHovered] = ImVec4(0.27f, 0.27f, 0.27f, 1.00f);
            colors[ImGuiCol_FrameBgActive] = ImVec4(0.255f, 0.255f, 0.275f, 1.00f);
            colors[ImGuiCol_TitleBg] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
            colors[ImGuiCol_TitleBgActive] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
            colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
            colors[ImGuiCol_MenuBarBg] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
            colors[ImGuiCol_ScrollbarBg] = ImVec4(0.074f, 0.074f, 0.074f, 1.0f);
            colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.31f, 0.31f, 0.32f, 1.00f);
            colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.41f, 0.41f, 0.42f, 1.00f);
            colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.51f, 0.51f, 0.53f, 1.00f);
            colors[ImGuiCol_CheckMark] = ImVec4(0.44f, 0.44f, 0.47f, 1.00f);
            colors[ImGuiCol_SliderGrab] = ImVec4(0.44f, 0.44f, 0.47f, 1.00f);
            colors[ImGuiCol_SliderGrabActive] = ImVec4(0.59f, 0.59f, 0.61f, 1.00f);
            colors[ImGuiCol_Button] = ImVec4(0.20f, 0.20f, 0.22f, 1.00f);
            colors[ImGuiCol_ButtonHovered] = ImVec4(0.44f, 0.44f, 0.47f, 1.00f);
            colors[ImGuiCol_ButtonActive] = ImVec4(0.59f, 0.59f, 0.61f, 1.00f);
            colors[ImGuiCol_Header] = ImVec4(0.20f, 0.20f, 0.22f, 1.00f);
            colors[ImGuiCol_HeaderHovered] = ImVec4(0.44f, 0.44f, 0.47f, 1.00f);
            colors[ImGuiCol_HeaderActive] = ImVec4(0.59f, 0.59f, 0.61f, 1.00f);
            colors[ImGuiCol_Separator] = ImVec4(1.00f, 1.00f, 1.00f, 0.20f);
            colors[ImGuiCol_SeparatorHovered] = ImVec4(0.44f, 0.44f, 0.47f, 0.39f);
            colors[ImGuiCol_SeparatorActive] = ImVec4(0.44f, 0.44f, 0.47f, 0.59f);
            colors[ImGuiCol_ResizeGrip] = ImVec4(0.26f, 0.59f, 0.98f, 0.00f);
            colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.00f);
            colors[ImGuiCol_ResizeGripActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.00f);
            colors[ImGuiCol_Tab] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
            colors[ImGuiCol_TabHovered] = ImVec4(0.00f, 0.48f, 0.80f, 1.00f);
            colors[ImGuiCol_TabActive] = ImVec4(0.00f, 0.48f, 0.80f, 1.00f);
            colors[ImGuiCol_TabUnfocused] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
            colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.00f, 0.48f, 0.80f, 1.00f);
            colors[ImGuiCol_DockingPreview] = ImVec4(0.00f, 0.48f, 0.80f, 1.00f);
            colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.114f, 0.114f, 0.125f, 1.0f);
            colors[ImGuiCol_PlotLines] = ImVec4(0.96f, 0.96f, 0.99f, 1.00f);
            colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.12f, 1.00f, 0.12f, 1.00f);
            colors[ImGuiCol_PlotHistogram] = ImVec4(0.23f, 0.51f, 0.86f, 1.00f);
            colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.12f, 1.00f, 0.12f, 1.00f);
            colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
            colors[ImGuiCol_DragDropTarget] = ImVec4(0.26f, 0.59f, 0.98f, 0.00f);
            colors[ImGuiCol_NavHighlight] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
            colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
            colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
            colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
            colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
            break;

        case Orange:
            //Start with imgui dark theme
            SecondaryTextColor = { 0.82f, 0.49f, 0.02f, 1.00f };
            ImGui::StyleColorsDark();
            style->WindowPadding = ImVec2(8, 8);
            style->FramePadding = ImVec2(5, 5);
            style->ItemSpacing = ImVec2(8, 8);
            style->ItemInnerSpacing = ImVec2(8, 6);
            style->IndentSpacing = 25.0f;
            style->ScrollbarSize = 18.0f;
            style->GrabMinSize = 12.0f;
            style->WindowBorderSize = 0.0f;
            style->ChildBorderSize = 1.0f;
            style->PopupBorderSize = 1.0f;
            style->FrameBorderSize = 1.0f;
            style->TabBorderSize = 0.0f;
            style->WindowRounding = 4.0f;
            style->ChildRounding = 0.0f;
            style->FrameRounding = 4.0f;
            style->PopupRounding = 4.0f;
            style->ScrollbarRounding = 4.0f;
            style->GrabRounding = 4.0f;
            style->TabRounding = 4.0f;

            colors[ImGuiCol_Text] = ImVec4(0.96f, 0.96f, 0.99f, 1.00f);
            colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
            colors[ImGuiCol_WindowBg] = ImVec4(0.114f, 0.114f, 0.125f, 1.0f);
            colors[ImGuiCol_ChildBg] = ImVec4(0.106f, 0.106f, 0.118f, 1.0f);
            colors[ImGuiCol_PopupBg] = ImVec4(0.06f, 0.06f, 0.07f, 1.00f);
            colors[ImGuiCol_Border] = ImVec4(0.216f, 0.216f, 0.216f, 1.0f);
            colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
            colors[ImGuiCol_FrameBg] = ImVec4(0.161f, 0.161f, 0.176f, 1.00f);
            colors[ImGuiCol_FrameBgHovered] = ImVec4(0.216f, 0.216f, 0.235f, 1.00f);
            colors[ImGuiCol_FrameBgActive] = ImVec4(0.255f, 0.255f, 0.275f, 1.00f);
            colors[ImGuiCol_TitleBg] = ImVec4(0.157f, 0.157f, 0.157f, 1.0f);
            colors[ImGuiCol_TitleBgActive] = ImVec4(0.216f, 0.216f, 0.216f, 1.0f);
            colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.157f, 0.157f, 0.157f, 1.0f);
            colors[ImGuiCol_MenuBarBg] = ImVec4(0.157f, 0.157f, 0.157f, 1.0f);
            colors[ImGuiCol_ScrollbarBg] = ImVec4(0.074f, 0.074f, 0.074f, 1.0f);
            colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.31f, 0.31f, 0.32f, 1.00f);
            colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.41f, 0.41f, 0.42f, 1.00f);
            colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.51f, 0.51f, 0.53f, 1.00f);
            colors[ImGuiCol_CheckMark] = ImVec4(0.44f, 0.44f, 0.47f, 1.00f);
            colors[ImGuiCol_SliderGrab] = ImVec4(0.44f, 0.44f, 0.47f, 1.00f);
            colors[ImGuiCol_SliderGrabActive] = ImVec4(0.59f, 0.59f, 0.61f, 1.00f);
            colors[ImGuiCol_Button] = ImVec4(0.20f, 0.20f, 0.22f, 1.00f);
            colors[ImGuiCol_ButtonHovered] = ImVec4(0.44f, 0.44f, 0.47f, 1.00f);
            colors[ImGuiCol_ButtonActive] = ImVec4(0.59f, 0.59f, 0.61f, 1.00f);
            colors[ImGuiCol_Header] = ImVec4(0.20f, 0.20f, 0.22f, 1.00f);
            colors[ImGuiCol_HeaderHovered] = ImVec4(0.44f, 0.44f, 0.47f, 1.00f);
            colors[ImGuiCol_HeaderActive] = ImVec4(0.59f, 0.59f, 0.61f, 1.00f);
            colors[ImGuiCol_Separator] = ImVec4(1.00f, 1.00f, 1.00f, 0.20f);
            colors[ImGuiCol_SeparatorHovered] = ImVec4(0.44f, 0.44f, 0.47f, 0.39f);
            colors[ImGuiCol_SeparatorActive] = ImVec4(0.44f, 0.44f, 0.47f, 0.59f);
            colors[ImGuiCol_ResizeGrip] = ImVec4(0.75f, 0.45f, 0.02f, 1.00f);
            colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.85f, 0.51f, 0.02f, 1.00f);
            colors[ImGuiCol_ResizeGripActive] = ImVec4(0.75f, 0.45f, 0.02f, 1.00f);
            colors[ImGuiCol_Tab] = ImVec4(0.21f, 0.21f, 0.24f, 1.00f);
            colors[ImGuiCol_TabHovered] = ImVec4(0.85f, 0.51f, 0.02f, 1.00f);
            colors[ImGuiCol_TabActive] = ImVec4(0.75f, 0.45f, 0.02f, 1.00f);
            colors[ImGuiCol_TabUnfocused] = ImVec4(0.21f, 0.21f, 0.24f, 1.00f);
            colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.75f, 0.45f, 0.02f, 1.00f);
            colors[ImGuiCol_DockingPreview] = ImVec4(0.75f, 0.45f, 0.02f, 1.00f);
            colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.114f, 0.114f, 0.125f, 1.0f);
            colors[ImGuiCol_PlotLines] = ImVec4(0.96f, 0.96f, 0.99f, 1.00f);
            colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.12f, 1.00f, 0.12f, 1.00f);
            colors[ImGuiCol_PlotHistogram] = ImVec4(0.75f, 0.45f, 0.02f, 1.00f);
            colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.12f, 1.00f, 0.12f, 1.00f);
            colors[ImGuiCol_TextSelectedBg] = ImVec4(0.75f, 0.45f, 0.02f, 1.00f);
            colors[ImGuiCol_DragDropTarget] = ImVec4(0.85f, 0.51f, 0.02f, 1.00f);
            colors[ImGuiCol_NavHighlight] = ImVec4(0.75f, 0.45f, 0.02f, 1.00f);
            colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
            colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
            colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
            break;

        case Blue:
            //Start with imgui dark theme
            ImGui::StyleColorsDark();
            SecondaryTextColor = { 0.32f, 0.67f, 1.0f, 1.00f }; //Light blue
            style->WindowPadding = ImVec2(8, 8);
            style->FramePadding = ImVec2(5, 5);
            style->ItemSpacing = ImVec2(8, 8);
            style->ItemInnerSpacing = ImVec2(8, 6);
            style->IndentSpacing = 25.0f;
            style->ScrollbarSize = 18.0f;
            style->GrabMinSize = 12.0f;
            style->WindowBorderSize = 0.0f;
            style->ChildBorderSize = 1.0f;
            style->PopupBorderSize = 1.0f;
            style->FrameBorderSize = 1.0f;
            style->TabBorderSize = 0.0f;
            style->WindowRounding = 4.0f;
            style->ChildRounding = 0.0f;
            style->FrameRounding = 4.0f;
            style->PopupRounding = 4.0f;
            style->ScrollbarRounding = 4.0f;
            style->GrabRounding = 4.0f;
            style->TabRounding = 4.0f;

            colors[ImGuiCol_Text] = ImVec4(0.95f, 0.96f, 0.98f, 1.00f);
            colors[ImGuiCol_TextDisabled] = ImVec4(0.36f, 0.42f, 0.47f, 1.00f);
            colors[ImGuiCol_WindowBg] = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
            colors[ImGuiCol_ChildBg] = ImVec4(0.15f, 0.18f, 0.22f, 1.00f);
            colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
            colors[ImGuiCol_Border] = ImVec4(0.08f, 0.10f, 0.12f, 1.00f);
            colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
            colors[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
            colors[ImGuiCol_FrameBgHovered] = ImVec4(0.278f, 0.337f, 0.384f, 1.00f);
            colors[ImGuiCol_FrameBgActive] = ImVec4(0.34f, 0.416f, 0.475f, 1.00f);
            colors[ImGuiCol_TitleBg] = ImVec4(0.09f, 0.12f, 0.14f, 0.65f);
            colors[ImGuiCol_TitleBgActive] = ImVec4(0.08f, 0.10f, 0.12f, 1.00f);
            colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
            colors[ImGuiCol_MenuBarBg] = ImVec4(0.15f, 0.18f, 0.22f, 1.00f);
            colors[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.39f);
            colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
            colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.278f, 0.337f, 0.384f, 1.00f);
            colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.34f, 0.416f, 0.475f, 1.00f);
            colors[ImGuiCol_CheckMark] = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
            colors[ImGuiCol_SliderGrab] = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
            colors[ImGuiCol_SliderGrabActive] = ImVec4(0.37f, 0.61f, 1.00f, 1.00f);
            colors[ImGuiCol_Button] = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
            colors[ImGuiCol_ButtonHovered] = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
            colors[ImGuiCol_ButtonActive] = ImVec4(0.06f, 0.53f, 0.98f, 1.00f);
            colors[ImGuiCol_Header] = ImVec4(0.20f, 0.25f, 0.29f, 0.55f);
            colors[ImGuiCol_HeaderHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
            colors[ImGuiCol_HeaderActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
            colors[ImGuiCol_Separator] = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
            colors[ImGuiCol_SeparatorHovered] = ImVec4(0.10f, 0.40f, 0.75f, 0.78f);
            colors[ImGuiCol_SeparatorActive] = ImVec4(0.10f, 0.40f, 0.75f, 1.00f);
            colors[ImGuiCol_ResizeGrip] = ImVec4(0.26f, 0.59f, 0.98f, 0.25f);
            colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
            colors[ImGuiCol_ResizeGripActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
            colors[ImGuiCol_Tab] = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
            colors[ImGuiCol_TabHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
            colors[ImGuiCol_TabActive] = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
            colors[ImGuiCol_TabUnfocused] = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
            colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
            colors[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
            colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
            colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
            colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
            colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
            colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
            colors[ImGuiCol_NavHighlight] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
            colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
            colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
            colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
            colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.15f, 0.18f, 0.22f, 1.00f);
            break;

        default:
            break;
        }
    }
#pragma warning(default:4505)
}