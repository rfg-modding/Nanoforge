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

//TODO: Add scene views
//void DX11Renderer::ViewportsDoFrame()
//{
//    ////On first ever draw set the viewport size to the default one. Only happens if the viewport window doesn't have a .ini entry
//    //if (!ImGui::Begin("Scene view"))
//    //{
//    //    ImGui::End();
//    //}
//    //
//    //ImVec2 contentAreaSize;
//    //contentAreaSize.x = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;
//    //contentAreaSize.y = ImGui::GetWindowContentRegionMax().y - ImGui::GetWindowContentRegionMin().y;
//    //ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(clearColor.x, clearColor.y, clearColor.z, clearColor.w));
//
//    //Scene->Resize(contentAreaSize.x, contentAreaSize.y);
//
//    ////Render scene texture
//    //ImGui::Image(sceneViewShaderResource_, ImVec2(static_cast<f32>(sceneViewWidth_), static_cast<f32>(sceneViewHeight_)));
//    //ImGui::PopStyleColor();
//
//    //ImGui::End();
//}

void SceneDocument_Init(GuiState* state, Document& doc)
{
    //Get parent packfile
    SceneDocumentData* data = (SceneDocumentData*)doc.Data;

    //Todo: Create worker thread for this territory
    //Todo: Tell renderer to create new meshes and resources each time a terrain mesh is ready. Can do this in SceneDocument_Update()
}

void SceneDocument_Update(GuiState* state, Document& doc)
{
    if (!ImGui::Begin(doc.Title.c_str(), &doc.Open))
    {
        ImGui::End();
        return;
    }
    SceneDocumentData* data = (SceneDocumentData*)doc.Data;
    Scene* scene = state->Renderer->Scenes[data->SceneIndex];

    //Update current territory so zone list shows correct data
    if (ImGui::IsWindowAppearing())
        state->SetTerritory(data->TerritoryName);


    ImVec2 contentAreaSize;
    contentAreaSize.x = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;
    contentAreaSize.y = ImGui::GetWindowContentRegionMax().y - ImGui::GetWindowContentRegionMin().y;

    scene->Resize(contentAreaSize.x, contentAreaSize.y);

    //Render scene texture
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(clearColor.x, clearColor.y, clearColor.z, clearColor.w));
    ImGui::Image(scene->GetView(), ImVec2(static_cast<f32>(scene->Width()), static_cast<f32>(scene->Height())));
    ImGui::PopStyleColor();

    ImGui::End();
}

void SceneDocument_OnClose(GuiState* state, Document& doc)
{
    SceneDocumentData* data = (SceneDocumentData*)doc.Data;

    //Free document data
    delete data;
}