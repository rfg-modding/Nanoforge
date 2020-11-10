#include "StaticMeshDocument.h"
#include "render/backend/DX11Renderer.h"
#include "util/RfgUtil.h"
#include "Log.h"

#ifdef DEBUG_BUILD
const string staticMeshShaderPath_ = "C:/Users/moneyl/source/repos/Project28/Assets/shaders/StaticMesh.fx";
#else
const string staticMeshShaderPath_ = "./Assets/shaders/StaticMesh.fx";
#endif

void StaticMeshDocument_WorkerThread(GuiState* state, Document& doc);

void StaticMeshDocument_Init(GuiState* state, Document& doc)
{
    StaticMeshDocumentData* data = (StaticMeshDocumentData*)doc.Data;

    //Create scene instance and store index
    data->SceneIndex = state->Renderer->Scenes.size();
    state->Renderer->CreateScene();

    //Init scene camera
    Scene& scene = state->Renderer->Scenes[data->SceneIndex];
    scene.Cam.Init({ 10.0f, 10.0f, 10.0f }, 80.0f, { (f32)scene.Width(), (f32)scene.Height() }, 1.0f, 10000.0f);
    scene.Cam.Speed = 0.25f;
    scene.SetShader(staticMeshShaderPath_);
    scene.SetVertexLayout //Todo: Vary this based on vertex format of static mesh we opened
    ({
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0,  DXGI_FORMAT_R8G8B8A8_UNORM, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TANGENT", 0,  DXGI_FORMAT_R8G8B8A8_UNORM, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "UV", 0,  DXGI_FORMAT_R16G16_UNORM, 0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    });

    //Create worker thread to load terrain meshes in background
    //data->WorkerFuture = std::async(std::launch::async, &StaticMeshDocument_WorkerThread, state, doc);

    //Todo: Move into worker thread once working. Just here for prototyping
    //Get gpu filename
    string gpuFileName = RfgUtil::CpuFilenameToGpuFilename(data->Filename);

    //Get path to cpu file and gpu file in cache
    data->CpuFilePath = data->InContainer ?
        state->PackfileVFS->GetFile(data->VppName, data->ParentName, data->Filename) :
        state->PackfileVFS->GetFile(data->VppName, data->Filename);
    data->GpuFilePath = data->InContainer ?
        state->PackfileVFS->GetFile(data->VppName, data->ParentName, gpuFileName) :
        state->PackfileVFS->GetFile(data->VppName, gpuFileName);

    //Read cpu file
    BinaryReader cpuFileReader(data->CpuFilePath);
    BinaryReader gpuFileReader(data->GpuFilePath);
    data->StaticMesh.Read(cpuFileReader, data->Filename);
    //Read index and vertex buffers from gpu file
    auto maybeMeshData = data->StaticMesh.ReadSubmeshData(gpuFileReader);
    if (!maybeMeshData)
        THROW_EXCEPTION("Failed to get mesh data for static mesh doc in StaticMesh::ReadSubmeshData()");

    MeshInstanceData meshData = maybeMeshData.value();
    auto& renderObject = scene.Objects.emplace_back();
    Mesh mesh;
    mesh.Create(scene.d3d11Device_, scene.d3d11Context_, meshData.VertexBuffer, meshData.IndexBuffer,
        data->StaticMesh.VertexBufferConfig.NumVerts, DXGI_FORMAT_R16_UINT, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    renderObject.Create(mesh, Vec3{ 0.0f, 0.0f, 0.0f });
    renderObject.SetScale(25.0f);

    //Clear mesh data
    delete[] meshData.IndexBuffer.data();
    delete[] meshData.VertexBuffer.data();
}

void StaticMeshDocument_DrawOverlayButtons(GuiState* state, Document& doc);

void StaticMeshDocument_Update(GuiState* state, Document& doc)
{
    StaticMeshDocumentData* data = (StaticMeshDocumentData*)doc.Data;
    Scene& scene = state->Renderer->Scenes[data->SceneIndex];

    if (!ImGui::Begin(doc.Title.c_str(), &doc.Open))
    {
        ImGui::End();
        return;
    }

    //Camera only handles input if window is focused
    scene.Cam.InputActive = ImGui::IsWindowFocused();

    ImVec2 contentAreaSize;
    contentAreaSize.x = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;
    contentAreaSize.y = ImGui::GetWindowContentRegionMax().y - ImGui::GetWindowContentRegionMin().y;
    scene.HandleResize(contentAreaSize.x, contentAreaSize.y);

    //Store initial position so we can draw buttons over the scene texture after drawing it
    ImVec2 initialPos = ImGui::GetCursorPos();

    //Render scene texture
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(scene.ClearColor.x, scene.ClearColor.y, scene.ClearColor.z, scene.ClearColor.w));
    ImGui::Image(scene.GetView(), ImVec2(static_cast<f32>(scene.Width()), static_cast<f32>(scene.Height())));
    ImGui::PopStyleColor();

    //Set cursor pos to top left corner to draw buttons over scene texture
    ImVec2 adjustedPos = initialPos;
    adjustedPos.x += 10.0f;
    adjustedPos.y += 10.0f;
    ImGui::SetCursorPos(adjustedPos);

    StaticMeshDocument_DrawOverlayButtons(state, doc);

    ImGui::End();
}

void StaticMeshDocument_DrawOverlayButtons(GuiState* state, Document& doc)
{
    StaticMeshDocumentData* data = (StaticMeshDocumentData*)doc.Data;
    Scene& scene = state->Renderer->Scenes[data->SceneIndex];

    state->FontManager->FontL.Push();
    if (ImGui::Button(ICON_FA_CAMERA))
        ImGui::OpenPopup("##CameraSettingsPopup");
    state->FontManager->FontL.Pop();
    if (ImGui::BeginPopup("##CameraSettingsPopup"))
    {
        state->FontManager->FontL.Push();
        ImGui::Text("Camera");
        state->FontManager->FontL.Pop();
        ImGui::Separator();

        f32 fov = scene.Cam.GetFov();
        f32 nearPlane = scene.Cam.GetNearPlane();
        f32 farPlane = scene.Cam.GetFarPlane();
        f32 lookSensitivity = scene.Cam.GetLookSensitivity();

        if (ImGui::Button("0.1")) scene.Cam.Speed = 0.1f;
        ImGui::SameLine();
        if (ImGui::Button("1.0")) scene.Cam.Speed = 1.0f;
        ImGui::SameLine();
        if (ImGui::Button("10.0")) scene.Cam.Speed = 10.0f;
        ImGui::SameLine();
        if (ImGui::Button("25.0")) scene.Cam.Speed = 25.0f;
        ImGui::SameLine();
        if (ImGui::Button("50.0")) scene.Cam.Speed = 50.0f;
        ImGui::SameLine();
        if (ImGui::Button("100.0")) scene.Cam.Speed = 100.0f;

        ImGui::InputFloat("Speed", &scene.Cam.Speed);
        ImGui::InputFloat("Sprint speed", &scene.Cam.SprintSpeed);
        ImGui::Separator();

        if (ImGui::InputFloat("Fov", &fov))
            scene.Cam.SetFov(fov);
        if (ImGui::InputFloat("Near plane", &nearPlane))
            scene.Cam.SetNearPlane(nearPlane);
        if (ImGui::InputFloat("Far plane", &farPlane))
            scene.Cam.SetFarPlane(farPlane);
        if (ImGui::InputFloat("Look sensitivity", &lookSensitivity))
            scene.Cam.SetLookSensitivity(lookSensitivity);

        ImGui::Separator();
        if (ImGui::InputFloat3("Position", (float*)&scene.Cam.camPosition))
        {
            scene.Cam.UpdateViewMatrix();
        }
        ImGui::EndPopup();
    }

    ImGui::SameLine();
    state->FontManager->FontL.Push();
    if (ImGui::Button(ICON_FA_SUN))
        ImGui::OpenPopup("##SceneSettingsPopup");
    state->FontManager->FontL.Pop();
    if (ImGui::BeginPopup("##SceneSettingsPopup"))
    {
        state->FontManager->FontL.Push();
        ImGui::Text("Render settings");
        state->FontManager->FontL.Pop();
        ImGui::Separator();

        ImGui::SetNextItemWidth(175.0f);
        static float tempScale = 1.0f;
        ImGui::InputFloat("Scale", &tempScale);
        ImGui::SameLine();
        if (ImGui::Button("Set all"))
        {
            scene.Objects[0].Scale.x = tempScale;
            scene.Objects[0].Scale.y = tempScale;
            scene.Objects[0].Scale.z = tempScale;
        }
        ImGui::DragFloat3("Scale", (float*)&scene.Objects[0].Scale, 0.01, 1.0f, 100.0f);

        ImGui::Text("Shading mode: ");
        ImGui::SameLine();
        ImGui::RadioButton("Elevation", &scene.perFrameStagingBuffer_.ShadeMode, 0);
        ImGui::SameLine();
        ImGui::RadioButton("Diffuse", &scene.perFrameStagingBuffer_.ShadeMode, 1);

        if (scene.perFrameStagingBuffer_.ShadeMode != 0)
        {
            ImGui::Text("Diffuse presets: ");
            ImGui::SameLine();
            if (ImGui::Button("Default"))
            {
                scene.perFrameStagingBuffer_.DiffuseColor = { 1.0f, 1.0f, 1.0f, 1.0f };
                scene.perFrameStagingBuffer_.DiffuseIntensity = 0.65f;
                scene.perFrameStagingBuffer_.ElevationFactorBias = 0.8f;
            }
            ImGui::SameLine();
            if (ImGui::Button("False color"))
            {
                scene.perFrameStagingBuffer_.DiffuseColor = { 1.15f, 0.67f, 0.02f, 1.0f };
                scene.perFrameStagingBuffer_.DiffuseIntensity = 0.55f;
                scene.perFrameStagingBuffer_.ElevationFactorBias = -0.8f;
            }

            ImGui::ColorEdit3("Diffuse", reinterpret_cast<f32*>(&scene.perFrameStagingBuffer_.DiffuseColor));
            ImGui::InputFloat("Diffuse intensity", &scene.perFrameStagingBuffer_.DiffuseIntensity);
            ImGui::InputFloat("Elevation color bias", &scene.perFrameStagingBuffer_.ElevationFactorBias);
        }

        ImGui::EndPopup();
    }
}

void StaticMeshDocument_OnClose(GuiState* state, Document& doc)
{
    StaticMeshDocumentData* data = (StaticMeshDocumentData*)doc.Data;
    Scene& scene = state->Renderer->Scenes[data->SceneIndex];

    //Delete scene and free its resources
    state->Renderer->DeleteScene(data->SceneIndex);

    //Free document data
    delete data;
}

void StaticMeshDocument_WorkerThread(GuiState* state, Document& doc)
{
    StaticMeshDocumentData* data = (StaticMeshDocumentData*)doc.Data;
    Scene& scene = state->Renderer->Scenes[data->SceneIndex];


}