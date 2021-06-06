#include "StaticMeshDocument.h"
#include "render/backend/DX11Renderer.h"
#include "util/RfgUtil.h"
#include "common/filesystem/Path.h"
#include "common/string/String.h"
#include "RfgTools++/formats/textures/PegFile10.h"
#include "Log.h"
#include "gui/documents/PegHelpers.h"
#include "render/imgui/imgui_ext.h"
#include "gui/util/WinUtil.h"
#include "PegHelpers.h"
#include <imgui_internal.h>
#include <optional>


StaticMeshDocument::StaticMeshDocument(GuiState* state, string filename, string parentName, string vppName, bool inContainer)
    : Filename(filename), ParentName(parentName), VppName(vppName), InContainer(inContainer)
{
    state_ = state;

    //Create scene instance and store index
    Scene = state->Renderer->CreateScene();
    if (!Scene)
        THROW_EXCEPTION("Failed to create scene in StaticmeshDocument constructor! Filename: {}", filename);

    //Init scene and camera
    Scene->Cam.Init({ 7.5f, 15.0f, 12.0f }, 80.0f, { (f32)Scene->Width(), (f32)Scene->Height() }, 1.0f, 10000.0f);
    Scene->Cam.Speed = 0.25f;
    Scene->Cam.SprintSpeed = 0.4f;
    Scene->Cam.LookAt({ 0.0f, 0.0f, 0.0f });

    //Create worker thread to load terrain meshes in background
    WorkerFuture = std::async(std::launch::async, &StaticMeshDocument::WorkerThread, this, state);
}

StaticMeshDocument::~StaticMeshDocument()
{
    //Wait for worker thread to so we don't destroy resources it's using
    open_ = false;
    WorkerFuture.wait();

    //Delete scene and free its resources
    state_->Renderer->DeleteScene(Scene);
    Scene = nullptr;
}

void StaticMeshDocument::Update(GuiState* state)
{
    if (!ImGui::Begin(Title.c_str(), &open_))
    {
        ImGui::End();
        return;
    }

    //Camera only handles input if window is focused
    Scene->Cam.InputActive = ImGui::IsWindowFocused();
    //Only redraw scene if window is focused
    Scene->NeedsRedraw = ImGui::IsWindowFocused();

    ImVec2 contentAreaSize;
    contentAreaSize.x = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;
    contentAreaSize.y = ImGui::GetWindowContentRegionMax().y - ImGui::GetWindowContentRegionMin().y;
    Scene->HandleResize(contentAreaSize.x, contentAreaSize.y);

    //Store initial position so we can draw buttons over the scene texture after drawing it
    ImVec2 initialPos = ImGui::GetCursorPos();

    //Render scene texture
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(Scene->ClearColor.x, Scene->ClearColor.y, Scene->ClearColor.z, Scene->ClearColor.w));
    ImGui::Image(Scene->GetView(), ImVec2(static_cast<f32>(Scene->Width()), static_cast<f32>(Scene->Height())));
    ImGui::PopStyleColor();

    //Set cursor pos to top left corner to draw buttons over scene texture
    ImVec2 adjustedPos = initialPos;
    adjustedPos.x += 10.0f;
    adjustedPos.y += 10.0f;
    ImGui::SetCursorPos(adjustedPos);

    DrawOverlayButtons(state);

    ImGui::End();
}

void StaticMeshDocument::DrawOverlayButtons(GuiState* state)
{
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

        //If popup is visible then redraw scene each frame. Simpler than trying to add checks for each option changing
        Scene->NeedsRedraw = true;

        f32 fov = Scene->Cam.GetFov();
        f32 nearPlane = Scene->Cam.GetNearPlane();
        f32 farPlane = Scene->Cam.GetFarPlane();
        f32 lookSensitivity = Scene->Cam.GetLookSensitivity();

        if (ImGui::Button("0.1")) Scene->Cam.Speed = 0.1f;
        ImGui::SameLine();
        if (ImGui::Button("1.0")) Scene->Cam.Speed = 1.0f;
        ImGui::SameLine();
        if (ImGui::Button("10.0")) Scene->Cam.Speed = 10.0f;
        ImGui::SameLine();
        if (ImGui::Button("25.0")) Scene->Cam.Speed = 25.0f;
        ImGui::SameLine();
        if (ImGui::Button("50.0")) Scene->Cam.Speed = 50.0f;
        ImGui::SameLine();
        if (ImGui::Button("100.0")) Scene->Cam.Speed = 100.0f;

        ImGui::InputFloat("Speed", &Scene->Cam.Speed);
        ImGui::InputFloat("Sprint speed", &Scene->Cam.SprintSpeed);
        ImGui::Separator();

        if (ImGui::InputFloat("Fov", &fov))
            Scene->Cam.SetFov(fov);
        if (ImGui::InputFloat("Near plane", &nearPlane))
            Scene->Cam.SetNearPlane(nearPlane);
        if (ImGui::InputFloat("Far plane", &farPlane))
            Scene->Cam.SetFarPlane(farPlane);
        if (ImGui::InputFloat("Look sensitivity", &lookSensitivity))
            Scene->Cam.SetLookSensitivity(lookSensitivity);

        ImGui::Separator();
        if (ImGui::InputFloat3("Position", (float*)&Scene->Cam.camPosition))
        {
            Scene->Cam.UpdateViewMatrix();
        }

        gui::LabelAndValue("Pitch:", std::to_string(Scene->Cam.GetPitch()));
        gui::LabelAndValue("Yaw:", std::to_string(Scene->Cam.GetYaw()));

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

        //If popup is visible then redraw scene each frame. Simpler than trying to add checks for each option changing
        Scene->NeedsRedraw = true;

        ImGui::SetNextItemWidth(175.0f);
        static float tempScale = 1.0f;
        ImGui::InputFloat("Scale", &tempScale);
        ImGui::SameLine();
        if (ImGui::Button("Set all"))
        {
            Scene->Objects[0].Scale.x = tempScale;
            Scene->Objects[0].Scale.y = tempScale;
            Scene->Objects[0].Scale.z = tempScale;
        }
        ImGui::DragFloat3("Scale", (float*)&Scene->Objects[0].Scale, 0.01, 1.0f, 100.0f);

        ImGui::ColorEdit3("Diffuse", reinterpret_cast<f32*>(&Scene->perFrameStagingBuffer_.DiffuseColor));
        ImGui::SliderFloat("Diffuse intensity", &Scene->perFrameStagingBuffer_.DiffuseIntensity, 0.0f, 1.0f);

        ImGui::EndPopup();
    }

    ImGui::SameLine();
    state->FontManager->FontL.Push();
    if (ImGui::Button(ICON_FA_INFO_CIRCLE))
        ImGui::OpenPopup("##MeshInfoPopup");
    state->FontManager->FontL.Pop();
    if (ImGui::BeginPopup("##MeshInfoPopup"))
    {
        //Header / general data
        state->FontManager->FontL.Push();
        ImGui::Text("Mesh info");
        state->FontManager->FontL.Pop();
        ImGui::Separator();

        gui::LabelAndValue("Header size:", std::to_string(StaticMesh.CpuDataSize) + " bytes");
        gui::LabelAndValue("Data size:", std::to_string(StaticMesh.GpuDataSize) + " bytes");
        gui::LabelAndValue("Num LODs:", std::to_string(StaticMesh.NumLods));
        gui::LabelAndValue("Num submeshes:", std::to_string(StaticMesh.NumSubmeshes));
        gui::LabelAndValue("Num materials:", std::to_string(StaticMesh.Header.NumMaterials));
        gui::LabelAndValue("Num vertices:", std::to_string(StaticMesh.VertexBufferConfig.NumVerts));
        gui::LabelAndValue("Num indices:", std::to_string(StaticMesh.IndexBufferConfig.NumIndices));
        gui::LabelAndValue("Vertex format:", to_string(StaticMesh.VertexBufferConfig.Format));
        gui::LabelAndValue("Vertex size:", std::to_string(StaticMesh.VertexBufferConfig.VertexStride0));
        gui::LabelAndValue("Index size:", std::to_string(StaticMesh.IndexBufferConfig.IndexSize));

        //Submesh data
        ImGui::Separator();
        state->FontManager->FontL.Push();
        ImGui::Text(ICON_FA_CUBES "Submeshes");
        state->FontManager->FontL.Pop();
        ImGui::Separator();

        //If popup is visible then redraw scene each frame. Simpler than trying to add checks for each option changing
        Scene->NeedsRedraw = true;

        //Buttons to show/hide all submeshes at once
        if (ImGui::Button("Show all"))
        {
            for (u32 i = 0; i < StaticMesh.SubMeshes.size(); i++)
            {
                RenderObject& renderObj = Scene->Objects[RenderObjectIndices[i]];
                renderObj.Visible = true;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Hide all"))
        {
            for (u32 i = 0; i < StaticMesh.SubMeshes.size(); i++)
            {
                RenderObject& renderObj = Scene->Objects[RenderObjectIndices[i]];
                renderObj.Visible = false;
            }
        }

        if (ImGui::BeginChild("##MeshInfoSubmeshesList", ImVec2(200.0f, 150.0f)))
        {
            ImGui::Columns(2);
            ImGui::SetColumnWidth(0, 100.0f);
            ImGui::SetColumnWidth(1, 100.0f);
            for (u32 i = 0; i < StaticMesh.SubMeshes.size(); i++)
            {
                SubmeshData& submesh = StaticMesh.SubMeshes[i];
                if (i >= RenderObjectIndices.size()) //Skip submeshes that aren't fully loaded yet
                    continue;

                RenderObject& renderObj = Scene->Objects[RenderObjectIndices[i]];
                if (ImGui::Selectable(("Submesh " + std::to_string(i)).c_str()))
                {
                    //Todo: Edge highlight / glow selected submesh
                }
                ImGui::NextColumn();
                ImGui::Checkbox(("##SubmeshVisibility" + std::to_string(i)).c_str(), &renderObj.Visible);
                ImGui::NextColumn();
            }
            ImGui::EndChild();
        }
        ImGui::Columns(1);
        ImGui::EndPopup();
    }

    ImGui::SameLine();
    state->FontManager->FontL.Push();
    if (ImGui::Button(ICON_FA_FILE_EXPORT))
        ImGui::OpenPopup("##MeshExportPopup");
    state->FontManager->FontL.Pop();

    if (ImGui::BeginPopup("##MeshExportPopup"))
    {
        state->FontManager->FontL.Push();
        ImGui::Text("Export mesh");
        state->FontManager->FontL.Pop();
        ImGui::Separator();

        //Todo: Support other export options
        //Output format radio selector
        ImGui::Text("Format: ");
        ImGui::SameLine();
        ImGui::RadioButton("Obj", true);

        static string MeshExportPath;
        ImGui::InputText("Export path", &MeshExportPath);
        ImGui::SameLine();
        if (ImGui::Button("..."))
        {
            auto result = OpenFolder("Select the output folder");
            if (!result)
                return;

            MeshExportPath = result.value();
        }

        //Draw export button that is disabled if the worker thread isn't done loading the mesh
        if (!WorkerDone)
        {
            ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
        }
        if (ImGui::Button("Export"))
        {
            if (!std::filesystem::exists(MeshExportPath))
            {
                Log->error("Failed to export {} to obj. Output folder \"{}\" does not exist.", StaticMesh.Name, MeshExportPath);
            }
            else
            {
                //Extract textures used by mesh and get their names
                string diffuseMapName = "";
                string specularMapPath = "";
                string normalMapPath = "";
                if (DiffuseMapPegPath != "")
                {
                    string cpuFilePath = DiffuseMapPegPath;
                    string gpuFilePath = Path::GetParentDirectory(cpuFilePath) + "\\" + RfgUtil::CpuFilenameToGpuFilename(cpuFilePath);
                    PegHelpers::ExportSingle(cpuFilePath, gpuFilePath, DiffuseTextureName, MeshExportPath + "\\");
                    diffuseMapName = String::Replace(DiffuseTextureName, ".tga", ".dds");
                }
                if (SpecularMapPegPath != "")
                {
                    string cpuFilePath = SpecularMapPegPath;
                    string gpuFilePath = Path::GetParentDirectory(cpuFilePath) + "\\" + RfgUtil::CpuFilenameToGpuFilename(cpuFilePath);
                    PegHelpers::ExportSingle(cpuFilePath, gpuFilePath, SpecularTextureName, MeshExportPath + "\\");
                    specularMapPath = String::Replace(SpecularTextureName, ".tga", ".dds");
                }
                if (NormalMapPegPath != "")
                {
                    string cpuFilePath = NormalMapPegPath;
                    string gpuFilePath = Path::GetParentDirectory(cpuFilePath) + "\\" + RfgUtil::CpuFilenameToGpuFilename(cpuFilePath);
                    PegHelpers::ExportSingle(cpuFilePath, gpuFilePath, NormalTextureName, MeshExportPath + "\\");
                    normalMapPath = String::Replace(NormalTextureName, ".tga", ".dds");
                }

                //Write mesh to obj
                StaticMesh.WriteToObj(GpuFilePath, MeshExportPath, diffuseMapName, specularMapPath, normalMapPath);
            }
        }
        if (!WorkerDone)
        {
            ImGui::PopItemFlag();
            ImGui::PopStyleVar();
        }
        ImGui::SameLine();
        gui::TooltipOnPrevious("You must wait for the mesh to be fully loaded to export it. See the progress bar to the right of the export panel.", state->FontManager->FontDefault.GetFont());

        ImGui::EndPopup();
    }

    //Progress bar and text state of worker thread
    ImGui::SameLine();
    ImVec2 tempPos = ImGui::GetCursorPos();
    ImGui::SetCursorPos(ImVec2(tempPos.x, tempPos.y - 5.0f));
    ImGui::Text(WorkerStatusString.c_str());
    ImGui::SetCursorPos(ImVec2(tempPos.x, tempPos.y + ImGui::GetFontSize() + 6.0f));
    ImGui::ProgressBar(WorkerProgressFraction, ImVec2(230.0f, ImGui::GetFontSize() * 1.1f));
}

void StaticMeshDocument::WorkerThread(GuiState* state)
{
    WorkerStatusString = "Parsing header...";

    //Todo: Move into worker thread once working. Just here for prototyping
    //Get gpu filename
    string gpuFileName = RfgUtil::CpuFilenameToGpuFilename(Filename);
    if (!open_) //Exit early check
        return;

    //Get path to cpu file and gpu file in cache
    auto maybeCpuFilePath = InContainer ?
        state->PackfileVFS->GetFilePath(VppName, ParentName, Filename) :
        state->PackfileVFS->GetFilePath(VppName, Filename);
    auto maybeGpuFilePath = InContainer ?
        state->PackfileVFS->GetFilePath(VppName, ParentName, gpuFileName) :
        state->PackfileVFS->GetFilePath(VppName, gpuFileName);

    //Error handling for when cpu or gpu file aren't found
    if (!maybeCpuFilePath)
    {
        Log->error("Static mesh worker thread encountered error! Failed to find cpu file: \"{}\" in \"{}\"", Filename, InContainer ? VppName + "/" + ParentName : VppName);
        WorkerStatusString = "Error encountered. Check log.";
        WorkerDone = true;
        return;
    }
    if (!maybeGpuFilePath)
    {
        Log->error("Static mesh worker thread encountered error! Failed to find gpu file: \"{}\" in \"{}\"", gpuFileName, InContainer ? VppName + "/" + ParentName : VppName);
        WorkerStatusString = "Error encountered. Check log.";
        WorkerDone = true;
        return;
    }
    if (!open_) //Exit early check
        return;

    CpuFilePath = maybeCpuFilePath.value();
    GpuFilePath = maybeGpuFilePath.value();

    //Read cpu file
    BinaryReader cpuFileReader(CpuFilePath);
    BinaryReader gpuFileReader(GpuFilePath);
    string ext = Path::GetExtension(Filename);
    //Todo: Move signature + version into class or helper function. Users of StaticMesh::Read shouldn't need to know these to use it
    if (ext == ".csmesh_pc")
        StaticMesh.Read(cpuFileReader, Filename, 0xC0FFEE11, 5);
    else if (ext == ".ccmesh_pc")
        StaticMesh.Read(cpuFileReader, Filename, 0xFAC351A9, 4);

    Log->info("Mesh vertex format: {}", to_string(StaticMesh.VertexBufferConfig.Format));

    //Check if the document was closed. If so, end worker thread early
    if (!open_)
        return;

    //Todo: Put this in renderer / RenderObject code somewhere so it can be reused by other mesh code
    //Vary input and shader based on vertex format
    WorkerStatusString = "Setting up scene...";
    VertexFormat format = StaticMesh.VertexBufferConfig.Format;
    state->Renderer->ContextMutex.lock(); //Lock ID3D11DeviceContext mutex. Only one thread allowed to access it at once
    Scene->SetShader(shaderFolderPath_ + to_string(format) + ".fx");
    if (format == VertexFormat::Pixlit1Uv)
    {
        Scene->SetVertexLayout
        ({
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0,  DXGI_FORMAT_R8G8B8A8_UNORM, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0,  DXGI_FORMAT_R16G16_SINT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            });
    }
    else if (format == VertexFormat::Pixlit1UvNmap)
    {
        Scene->SetVertexLayout
        ({
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0,  DXGI_FORMAT_R8G8B8A8_UNORM, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TANGENT", 0,  DXGI_FORMAT_R8G8B8A8_UNORM, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0,  DXGI_FORMAT_R16G16_SINT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            });
    }
    else if (format == VertexFormat::Pixlit1UvNmapCa)
    {
        Scene->SetVertexLayout
        ({
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0,  DXGI_FORMAT_R8G8B8A8_UNORM, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TANGENT", 0,  DXGI_FORMAT_R8G8B8A8_UNORM, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "BLENDWEIGHT", 0,  DXGI_FORMAT_R8G8B8A8_UNORM, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "BLENDINDEX", 0,  DXGI_FORMAT_R8G8B8A8_UINT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0,  DXGI_FORMAT_R16G16_SINT, 0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            });
    }
    else if (format == VertexFormat::Pixlit2UvNmap)
    {
        Scene->SetVertexLayout
        ({
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0,  DXGI_FORMAT_R8G8B8A8_UNORM, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TANGENT", 0,  DXGI_FORMAT_R8G8B8A8_UNORM, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0,  DXGI_FORMAT_R16G16_SINT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 1,  DXGI_FORMAT_R16G16_SINT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            });
    }
    else if (format == VertexFormat::Pixlit3UvNmap)
    {
        Scene->SetVertexLayout
        ({
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0,  DXGI_FORMAT_R8G8B8A8_UNORM, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TANGENT", 0,  DXGI_FORMAT_R8G8B8A8_UNORM, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0,  DXGI_FORMAT_R16G16_SINT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 1,  DXGI_FORMAT_R16G16_SINT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 2,  DXGI_FORMAT_R16G16_SINT, 0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            });
    }
    else if (format == VertexFormat::Pixlit3UvNmapCa)
    {
        Scene->SetVertexLayout
        ({
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0,  DXGI_FORMAT_R8G8B8A8_UNORM, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TANGENT", 0,  DXGI_FORMAT_R8G8B8A8_UNORM, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "BLENDWEIGHT", 0,  DXGI_FORMAT_R8G8B8A8_UNORM, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "BLENDINDEX", 0,  DXGI_FORMAT_R8G8B8A8_UINT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0,  DXGI_FORMAT_R16G16_SINT, 0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 1,  DXGI_FORMAT_R16G16_SINT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 2,  DXGI_FORMAT_R16G16_SINT, 0, 36, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            });
    }
    state->Renderer->ContextMutex.unlock();

    //Hide all submeshes except first if num submeshes = num lods. Generally the other submeshes are low lod meshes
    bool hideLowLodMeshes = StaticMesh.NumLods == StaticMesh.NumSubmeshes;

    //Track list of textures found and not found to avoid repeat searches
    std::unordered_map<string, Texture2D_Ext> foundTextures = {};
    std::unordered_map<string, int> missingTextures = {};

    //Two steps for each submesh: Get index/vertex buffers and find textures
    u32 numSteps = StaticMesh.SubMeshes.size() * 2;
    f32 stepSize = 1.0f / (f32)numSteps;

    for (u32 i = 0; i < StaticMesh.SubMeshes.size(); i++)
    {
        //Check if the document was closed. If so, end worker thread early
        if (!open_)
            return;

        WorkerStatusString = "Loading submesh " + std::to_string(i) + "...";

        //Read index and vertex buffers from gpu file
        auto maybeMeshData = StaticMesh.ReadSubmeshData(gpuFileReader, i);
        if (!maybeMeshData)
            THROW_EXCEPTION("Failed to get mesh data for static mesh doc in StaticMesh::ReadSubmeshData()");

        state->Renderer->ContextMutex.lock();
        MeshInstanceData meshData = maybeMeshData.value();
        auto& renderObject = Scene->Objects.emplace_back();
        RenderObjectIndices.push_back(Scene->Objects.size() - 1);
        Mesh mesh;
        mesh.Create(Scene->d3d11Device_, Scene->d3d11Context_, meshData.VertexBuffer, meshData.IndexBuffer,
            StaticMesh.VertexBufferConfig.NumVerts, DXGI_FORMAT_R16_UINT, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        renderObject.Create(mesh, Vec3{ 0.0f, 0.0f, 0.0f });
        renderObject.SetScale(25.0f);
        if (hideLowLodMeshes && i > 0)
            renderObject.Visible = false;
        state->Renderer->ContextMutex.unlock();

        WorkerProgressFraction += stepSize;

        //Todo: Better handle materials. This works okay but doesn't always properly apply textures to meshes with multiple submeshes with different materials
        WorkerStatusString = "Locating textures for submesh " + std::to_string(i) + "...";
        bool foundDiffuse = false;
        bool foundSpecular = false;
        bool foundNormal = false;
        for (auto& textureName : StaticMesh.TextureNames)
        {
            //Check if the document was closed. If so, end worker thread early
            if (!open_)
            {
                delete[] meshData.IndexBuffer.data();
                delete[] meshData.VertexBuffer.data();
                return;
            }

            string textureNameLower = String::ToLower(textureName);
            bool missing = missingTextures.find(textureNameLower) != missingTextures.end();
            if (missing) //Skip textures that weren't found in previous searches
                continue;

            if (!foundDiffuse && String::Contains(textureNameLower, "_d"))
            {
                std::optional<Texture2D_Ext> texture = {};
                bool notInCache = foundTextures.find(textureNameLower) == foundTextures.end();
                if (notInCache)
                    texture = FindTexture(state, textureNameLower, true);
                else
                    texture = foundTextures[textureNameLower];

                if (texture)
                {
                    if (notInCache)
                        Log->info("Found diffuse texture {} for {}", texture->Texture.Name, Filename);
                    else
                        Log->info("Using cached copy of {} for {}", texture->Texture.Name, Filename);

                    std::lock_guard<std::mutex> lock(state->Renderer->ContextMutex);
                    renderObject.UseTextures = true;
                    renderObject.DiffuseTexture = texture.value().Texture;
                    foundDiffuse = true;

                    //Set diffuse texture used for export as first one found
                    if (DiffuseMapPegPath == "")
                    {
                        DiffuseMapPegPath = texture.value().CpuFilePath;
                        DiffuseTextureName = texture.value().Texture.Name;
                    }

                    if (foundTextures.find(textureNameLower) == foundTextures.end())
                        foundTextures[textureNameLower] = texture.value();
                }
                else
                {
                    missingTextures[textureNameLower] = 0;
                    Log->warn("Failed to find diffuse texture {} for {}", textureNameLower, Filename);
                }
            }
            else if (!foundNormal && String::Contains(textureNameLower, "_n"))
            {
                std::optional<Texture2D_Ext> texture = {};
                bool notInCache = foundTextures.find(textureNameLower) == foundTextures.end();
                if (notInCache)
                    texture = FindTexture(state, textureNameLower, true);
                else
                    texture = foundTextures[textureNameLower];

                if (texture)
                {
                    if (notInCache)
                        Log->info("Found normal map {} for {}", texture->Texture.Name, Filename);
                    else
                        Log->info("Using cached copy of {} for {}", texture->Texture.Name, Filename);

                    std::lock_guard<std::mutex> lock(state->Renderer->ContextMutex);
                    renderObject.UseTextures = true;
                    renderObject.NormalTexture = texture.value().Texture;
                    foundNormal = true;

                    //Set normal texture used for export as first one found
                    if (NormalMapPegPath == "")
                    {
                        NormalMapPegPath = texture.value().CpuFilePath;
                        NormalTextureName = texture.value().Texture.Name;
                    }

                    if (foundTextures.find(textureNameLower) == foundTextures.end())
                        foundTextures[textureNameLower] = texture.value();
                }
                else
                {
                    missingTextures[textureNameLower] = 0;
                    Log->warn("Failed to find normal map {} for {}", textureNameLower, Filename);
                }
            }
            else if (!foundSpecular && String::Contains(textureNameLower, "_s"))
            {
                std::optional<Texture2D_Ext> texture = {};
                bool notInCache = foundTextures.find(textureNameLower) == foundTextures.end();
                if (notInCache)
                    texture = FindTexture(state, textureNameLower, true);
                else
                    texture = foundTextures[textureNameLower];

                if (texture)
                {
                    if (notInCache)
                        Log->info("Found specular map {} for {}", texture->Texture.Name, Filename);
                    else
                        Log->info("Using cached copy of {} for {}", texture->Texture.Name, Filename);

                    std::lock_guard<std::mutex> lock(state->Renderer->ContextMutex);
                    renderObject.UseTextures = true;
                    renderObject.SpecularTexture = texture.value().Texture;
                    foundSpecular = true;

                    //Set specular texture used for export as first one found
                    if (SpecularMapPegPath == "")
                    {
                        SpecularMapPegPath = texture.value().CpuFilePath;
                        SpecularTextureName = texture.value().Texture.Name;
                    }

                    if (foundTextures.find(textureNameLower) == foundTextures.end())
                        foundTextures[textureNameLower] = texture.value();
                }
                else
                {
                    missingTextures[textureNameLower] = 0;
                    Log->warn("Failed to find specular map {} for {}", textureNameLower, Filename);
                }
            }
        }

        WorkerProgressFraction += stepSize;

        //Clear mesh data
        delete[] meshData.IndexBuffer.data();
        delete[] meshData.VertexBuffer.data();
    }

    WorkerDone = true;
    WorkerStatusString = "Done! " ICON_FA_CHECK;
    Log->info("Worker thread for {} finished.", Title);
}

//Used by texture search functions to stop the search early if the document is closed
#define DocumentClosedCheck() if(!open_) { return {}; }

//Finds a texture and creates a directx texture resource from it. textureName is the textureName of a texture inside a cpeg/cvbm. So for example, sledgehammer_high_n.tga, which is in sledgehammer_high.cpeg_pc
//Will try to find a high res version of the texture first if lookForHighResVariant is true.
//Will return a default texture if the target isn't found.
std::optional<Texture2D_Ext> StaticMeshDocument::FindTexture(GuiState* state, const string& name, bool lookForHighResVariant)
{
    //Look for high res variant if requested and string fits high res search requirements
    if (lookForHighResVariant && String::Contains(name, "_low_"))
    {
        //Replace _low_ with _. This is the naming scheme I've seen many high res variants follow
        string highResName = String::Replace(name, "_low_", "_");
        auto texture = GetTexture(state, highResName);

        //Return high res variant if it was found
        if (texture)
            return texture;
    }
    //Check if the document was closed. If so, end worker thread early
    DocumentClosedCheck();

    //Else look for the specified texture
    return GetTexture(state, name);
}

std::optional<Texture2D_Ext> StaticMeshDocument::GetTexture(GuiState* state, const string& textureName)
{
    //First search current str2_pc if the mesh is inside one
    if (InContainer)
    {
        Packfile3* container = state->PackfileVFS->GetContainer(ParentName, VppName);
        if (container)
        {
            auto texture = GetTextureFromPackfile(state, container, textureName, true);
            delete container; //Delete container since they're loaded on demand

            //Return texture if it was found
            if (texture)
                return texture;
        }
    }

    //Then search parent vpp
    DocumentClosedCheck();
    auto parentSearchResult = GetTextureFromPackfile(state, state->PackfileVFS->GetPackfile(VppName), textureName);
    if (parentSearchResult)
        return parentSearchResult;

    //Last resort is to search dlc01_precache, terr01_l0, and terr01_l1
    DocumentClosedCheck();
    auto dlcPrecacheSearchResult = GetTextureFromPackfile(state, state->PackfileVFS->GetPackfile("dlc01_precache.vpp_pc"), textureName); 
    if (dlcPrecacheSearchResult)
        return dlcPrecacheSearchResult;

    DocumentClosedCheck();
    auto terrL0SearchResult = GetTextureFromPackfile(state, state->PackfileVFS->GetPackfile("terr01_l0.vpp_pc"), textureName);
    if (terrL0SearchResult)
        return terrL0SearchResult;

    DocumentClosedCheck();
    return GetTextureFromPackfile(state, state->PackfileVFS->GetPackfile("terr01_l1.vpp_pc"), textureName);
}

//Tries to find a cpeg with a subtexture with the provided name and create a Texture2D from it. Searches all cpeg/cvbm files in packfile. First checks pegs then searches in str2s
std::optional<Texture2D_Ext> StaticMeshDocument::GetTextureFromPackfile(GuiState* state, Packfile3* packfile, const string& textureName, bool isStr2)
{
    if (!packfile)
        return {};

    //First search top level cpeg/cvbm files
    for (u32 i = 0; i < packfile->Entries.size(); i++)
    {
        //Check if the document was closed. If so, end worker thread early
        if (!open_)
            return {};

        Packfile3Entry& entry = packfile->Entries[i];
        const char* entryName = packfile->EntryNames[i];
        string ext = Path::GetExtension(entryName);

        //Try to get texture from each cpeg/cvbm
        if (ext == ".cpeg_pc" || ext == ".cvbm_pc")
        {
            auto texture = GetTextureFromPeg(state, packfile->Name(), entryName, textureName, isStr2);
            if (texture)
                return texture;
        }
    }

    //Then search inside each str2 if this packfile isn't a str2
    if (!isStr2)
    {
        for (u32 i = 0; i < packfile->Entries.size(); i++)
        {
            //Check if the document was closed. If so, end worker thread early
            DocumentClosedCheck();

            Packfile3Entry& entry = packfile->Entries[i];
            const char* entryName = packfile->EntryNames[i];
            string ext = Path::GetExtension(entryName);

            //Try to get texture from each str2
            if (ext == ".str2_pc")
            {
                //Find container
                auto containerBytes = packfile->ExtractSingleFile(entryName, false);
                if (!containerBytes)
                    continue;

                //Parse container and get file byte buffer
                Packfile3 container(containerBytes.value());
                container.SetName(entryName);
                container.ReadMetadata();
                auto texture = GetTextureFromPackfile(state, &container, textureName, true);
                if (texture)
                    return texture;
            }
        }
    }

    //If didn't find texture at this point then failed. Return empty
    return {};
}

//Tries to open a cpeg/cvbm pegName in the provided packfile and create a Texture2D from a sub-texture with the name textureName
std::optional<Texture2D_Ext> StaticMeshDocument::GetTextureFromPeg(GuiState* state,
    const string& parentName, //If inContainer == true this is the name of str2_pc the peg is in. Else it is the same as VppName
    const string& pegName, //Name of the cpeg_pc or cvbm_pc file that holds the target texture
    const string& textureName, //Name of the target texture. E.g. "sledgehammer_d.tga"
    bool inContainer) //Whether or not the cpeg_pc/cvbm_pc is in a str2_pc file or not
{
    //Get gpu filename
    string gpuFilename = RfgUtil::CpuFilenameToGpuFilename(pegName);

    //Get path to cpu file and gpu file in cache
    auto maybeCpuFilePath = inContainer ?
        state->PackfileVFS->GetFilePath(VppName, parentName, pegName) :
        state->PackfileVFS->GetFilePath(VppName, pegName);
    auto maybeGpuFilePath = inContainer ?
        state->PackfileVFS->GetFilePath(VppName, parentName, gpuFilename) :
        state->PackfileVFS->GetFilePath(VppName, gpuFilename);

    //Error handling for when cpu or gpu file aren't found
    if (!maybeCpuFilePath || !maybeGpuFilePath)
        return {};

    string cpuFilePath = maybeCpuFilePath.value();
    string gpuFilePath = maybeGpuFilePath.value();

    //Parse peg file
    std::optional<Texture2D_Ext> out = {};
    BinaryReader cpuFileReader(cpuFilePath);
    BinaryReader gpuFileReader(gpuFilePath);
    PegFile10 peg;
    peg.Read(cpuFileReader, gpuFileReader);

    //See if target texture is in peg. If so extract it and create a Texture2D from it
    for (auto& entry : peg.Entries)
    {
        if (String::EqualIgnoreCase(entry.Name, textureName))
        {
            peg.ReadTextureData(gpuFileReader, entry);
            std::span<u8> textureData = entry.RawData;

            //Create and setup texture2d
            Texture2D texture2d;
            texture2d.Name = textureName;
            DXGI_FORMAT dxgiFormat = PegHelpers::PegFormatToDxgiFormat(entry.BitmapFormat);
            D3D11_SUBRESOURCE_DATA textureSubresourceData;
            textureSubresourceData.pSysMem = textureData.data();
            textureSubresourceData.SysMemSlicePitch = 0;
            textureSubresourceData.SysMemPitch = PegHelpers::CalcRowPitch(dxgiFormat, entry.Width, entry.Height);

            state->Renderer->ContextMutex.lock(); //Lock ID3D11DeviceContext mutex. Only one thread allowed to access it at once
            texture2d.Create(Scene->d3d11Device_, entry.Width, entry.Height, dxgiFormat, D3D11_BIND_SHADER_RESOURCE, &textureSubresourceData);
            texture2d.CreateShaderResourceView(); //Need shader resource view to use it in shader
            texture2d.CreateSampler(); //Need sampler too
            state->Renderer->ContextMutex.unlock();

            out = Texture2D_Ext{ .Texture = texture2d, .CpuFilePath = cpuFilePath };
            break;
        }
    }

    //Release allocated memory and return output
    peg.Cleanup();
    return out;
}