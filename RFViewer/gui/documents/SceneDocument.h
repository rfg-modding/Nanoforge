#pragma once
#include "common/Typedefs.h"
#include "Document.h"
#include "gui/GuiState.h"
#include "render/backend/DX11Renderer.h"

struct SceneDocumentData
{
    string TerritoryName;
    u32 SceneIndex;
};

void SceneDocument_Init(GuiState* state, Document& doc)
{
    //Get parent packfile
    SceneDocumentData* data = (SceneDocumentData*)doc.Data;

    //Todo: Create worker thread for this territory
    //Todo: Tell renderer to create new meshes and resources each time a terrain mesh is ready. Can do this in SceneDocument_Update()
        //Todo: Step 1: Have Scene class handle this. Make it specifically for rendering territories
        //Todo: Step 2: Make scene class generic. Split texture/shader/model code into own classes. Maybe add custom behavior by making Scene abstract or with func ptrs
    //Todo: CurrentTerritory currently only keeps data for one territory. Either add support for multiple territories or do one instance per territory
    //Todo: ZoneList needs to know which zones info to display. Maybe have SceneDocument own territory data and ZoneList draw based on SceneDocument
    //Todo: Update ZoneList, Properties, Camera, and Render settings to support multiples scenes
    //Todo: Consider providing different ui/menu for opening territories instead of the zone list. Is too hidden having it in the zone list. Maybe have territories panel
}

void SceneDocument_Update(GuiState* state, Document& doc)
{
    if (!ImGui::Begin(doc.Title.c_str(), &doc.Open))
    {
        ImGui::End();
        return;
    }
    SceneDocumentData* data = (SceneDocumentData*)doc.Data;
    Scene& scene = state->Renderer->Scenes[data->SceneIndex];

    //Update current territory so zone list shows correct data
    if (ImGui::IsWindowAppearing())
        state->SetTerritory(data->TerritoryName);


    ImVec2 contentAreaSize;
    contentAreaSize.x = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;
    contentAreaSize.y = ImGui::GetWindowContentRegionMax().y - ImGui::GetWindowContentRegionMin().y;

    scene.HandleResize(contentAreaSize.x, contentAreaSize.y);

    //Render scene texture
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(scene.ClearColor.x, scene.ClearColor.y, scene.ClearColor.z, scene.ClearColor.w));
    ImGui::Image(scene.GetView(), ImVec2(static_cast<f32>(scene.Width()), static_cast<f32>(scene.Height())));
    ImGui::PopStyleColor();

    ImGui::End();
}

void SceneDocument_OnClose(GuiState* state, Document& doc)
{
    SceneDocumentData* data = (SceneDocumentData*)doc.Data;

    //Free document data
    delete data;
}