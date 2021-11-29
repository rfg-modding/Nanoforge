#include "PropertyPanel.h"
#include "util/Profiler.h"
#include "gui/GuiState.h"
#include "imgui.h"
#include "render/imgui/ImGuiFontManager.h"

PropertyPanel::PropertyPanel()
{

}

PropertyPanel::~PropertyPanel()
{

}

void PropertyPanel::Update(GuiState* state, bool* open)
{
    PROFILER_FUNCTION();
    if (!ImGui::Begin("Properties", open))
    {
        ImGui::End();
        return;
    }

    state->FontManager->FontL.Push();
    ImGui::Text(ICON_FA_WRENCH " Properties");
    state->FontManager->FontL.Pop();
    ImGui::Separator();

    if (state->PropertyPanelContentFuncPtr)
    {
        state->PropertyPanelContentFuncPtr(state);
    }
    else
    {
        ImGui::TextWrapped("%s Click a file in the file explorer or another panel to see it's properties here.", ICON_FA_EXCLAMATION_CIRCLE);
    }

    ImGui::End();
}