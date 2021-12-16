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
#include "render/Render.h"
#include <imgui_internal.h>
#include <optional>
#include "util/Profiler.h"
#include "rfg/TextureIndex.h"
#include "render/imgui/ImGuiFontManager.h"
#include "render/resources/Scene.h"
#include "render/backend/DX11Renderer.h"
#include "util/TaskScheduler.h"
#include "gui/GuiState.h"
#include "rfg/PackfileVFS.h"
#include "RfgTools++/formats/packfiles/Packfile3.h"
#include "RfgTools++/hashes/HashGuesser.h"

StaticMeshDocument::StaticMeshDocument(GuiState* state, std::string_view filename, std::string_view parentName, std::string_view vppName, bool inContainer)
    : Filename(filename), ParentName(parentName), VppName(vppName), InContainer(inContainer)
{
    state_ = state;
    NoWindowPadding = true;

    //Create scene instance and store index
    Scene = state->Renderer->CreateScene();
    if (!Scene)
        THROW_EXCEPTION("Failed to create scene for static mesh document \"{}\"", filename);

    //Init scene and camera
    Scene->Cam.Init({ 7.5f, 15.0f, 12.0f }, 80.0f, { (f32)Scene->Width(), (f32)Scene->Height() }, 1.0f, 10000.0f);
    Scene->Cam.Speed = 0.25f;
    Scene->Cam.SprintSpeed = 0.4f;
    Scene->Cam.LookAt({ 0.0f, 0.0f, 0.0f });
    Scene->perFrameStagingBuffer_.DiffuseIntensity = 2.5f;

    //Create worker thread to load terrain meshes in background
    meshLoadTask_ = Task::Create(fmt::format("Loading {}...", filename));
    TaskScheduler::QueueTask(meshLoadTask_, std::bind(&StaticMeshDocument::WorkerThread, this, meshLoadTask_, state));
}

StaticMeshDocument::~StaticMeshDocument()
{
    //Delete scene and free its resources. CanClose() ensures worker thread is finished by the time we destroy the resources.
    state_->Renderer->DeleteScene(Scene);
    Scene = nullptr;
}

bool StaticMeshDocument::CanClose()
{
    //Can't close until the worker threads are finished. The doc will still disappear if the user closes it. It just won't get deleted until all the threads finish.
    return !meshLoadTask_->Running();
}

void StaticMeshDocument::OnClose(GuiState* state)
{
    meshLoadTask_->Cancel();
}

void StaticMeshDocument::Update(GuiState* state)
{
    PROFILER_FUNCTION();

    //Camera only handles input if window is focused
    Scene->Cam.InputActive = ImGui::IsWindowFocused();
    //Only redraw scene if window is focused
    Scene->NeedsRedraw = ImGui::IsWindowFocused();

    ImVec2 contentAreaSize;
    contentAreaSize.x = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;
    contentAreaSize.y = ImGui::GetWindowContentRegionMax().y - ImGui::GetWindowContentRegionMin().y;
    if(contentAreaSize.x > 0.0f && contentAreaSize.y > 0.0f)
        Scene->HandleResize((u32)contentAreaSize.x, (u32)contentAreaSize.y);

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
}

#pragma warning(disable:4100)
void StaticMeshDocument::Save(GuiState* state)
{

}
#pragma warning(default:4100)

void StaticMeshDocument::DrawOverlayButtons(GuiState* state)
{
    state->FontManager->FontL.Push();
    if (ImGui::Button(ICON_FA_CAMERA))
        ImGui::OpenPopup("##CameraSettingsPopup");
    state->FontManager->FontL.Pop();

    //Must manually set padding here since the parent window has padding disabled to get the viewport flush with the window border.
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 8.0f, 8.0f });
    if (ImGui::BeginPopup("##CameraSettingsPopup"))
    {
        state->FontManager->FontL.Push();
        ImGui::Text("Camera");
        state->FontManager->FontL.Pop();
        ImGui::Separator();

        //If popup is visible then redraw scene each frame. Simpler than trying to add checks for each option changing
        Scene->NeedsRedraw = true;

        f32 fov = Scene->Cam.GetFovDegrees();
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

        if (ImGui::SliderFloat("Fov", &fov, 40.0f, 120.0f))
            Scene->Cam.SetFovDegrees(fov);
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

        gui::LabelAndValue("Pitch:", std::to_string(Scene->Cam.GetPitchDegrees()));
        gui::LabelAndValue("Yaw:", std::to_string(Scene->Cam.GetYawDegrees()));

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
            Scene->Objects[0]->Scale.x = tempScale;
            Scene->Objects[0]->Scale.y = tempScale;
            Scene->Objects[0]->Scale.z = tempScale;
        }
        ImGui::DragFloat3("Scale", (float*)&Scene->Objects[0]->Scale, 0.01f, 1.0f, 100.0f);

        ImGui::ColorEdit3("Diffuse", reinterpret_cast<f32*>(&Scene->perFrameStagingBuffer_.DiffuseColor));
        ImGui::SliderFloat("Diffuse intensity", &Scene->perFrameStagingBuffer_.DiffuseIntensity, 0.0f, 3.0f);

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

        gui::LabelAndValue("Header size:", std::to_string(StaticMesh.MeshInfo.CpuDataSize) + " bytes");
        gui::LabelAndValue("Data size:", std::to_string(StaticMesh.MeshInfo.GpuDataSize) + " bytes");
        gui::LabelAndValue("Num LODs:", std::to_string(StaticMesh.NumLods));
        gui::LabelAndValue("Num submeshes:", std::to_string(StaticMesh.MeshInfo.NumSubmeshes));
        gui::LabelAndValue("Num materials:", std::to_string(StaticMesh.Header.NumMaterials));
        gui::LabelAndValue("Num vertices:", std::to_string(StaticMesh.MeshInfo.NumVertices));
        gui::LabelAndValue("Num indices:", std::to_string(StaticMesh.MeshInfo.NumIndices));
        gui::LabelAndValue("Vertex format:", to_string(StaticMesh.MeshInfo.VertFormat));
        gui::LabelAndValue("Vertex size:", std::to_string(StaticMesh.MeshInfo.VertexStride0));
        gui::LabelAndValue("Index size:", std::to_string(StaticMesh.MeshInfo.IndexSize));

        ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, ImGui::GetFontSize() * 1.40f); //Increase spacing to differentiate leaves from expanded contents.
        if (ImGui::TreeNode("Materials"))
        {
            MaterialBlock& materialBlock = StaticMesh.MaterialBlock;
            for (RfgMaterial& material : materialBlock.Materials)
            {
                std::optional<string> nameGuess = HashGuesser::GuessHashOriginString(material.NameChecksum);
                if (ImGui::TreeNode(nameGuess.has_value() ? nameGuess.value().c_str() : std::to_string(material.NameChecksum).c_str()))
                {
                    gui::LabelAndValue("Shader handle:", std::to_string(material.ShaderHandle));
                    gui::LabelAndValue("Name checksum:", std::to_string(material.NameChecksum));
                    gui::LabelAndValue("Material flags:", std::to_string(material.MaterialFlags));
                    gui::LabelAndValue("Num textures:", std::to_string(material.NumTextures));
                    gui::LabelAndValue("Num constants:", std::to_string(material.NumConstants));
                    gui::LabelAndValue("Max constants:", std::to_string(material.MaxConstants));
                    gui::LabelAndValue("Texture offset:", std::to_string(material.TextureOffset));
                    gui::LabelAndValue("Constant name checksums offset:", std::to_string(material.ConstantNameChecksumsOffset));
                    gui::LabelAndValue("Constant block offset:", std::to_string(material.ConstantBlockOffset));

                    if (ImGui::TreeNode("Constants"))
                    {
                        for (u32 i = 0; i < material.NumConstants; i++)
                        {
                            MaterialConstant& constant = material.Constants[i];
                            const u32 constantNameChecksum = material.ConstantNameChecksums[i];
                            std::optional<string> constantNameGuess = HashGuesser::GuessHashOriginString(constantNameChecksum);
                            string constantName = constantNameGuess.value_or(std::to_string(constantNameChecksum));
                            ImGui::InputFloat4(constantName.c_str(), constant.Constants);
                        }
                        ImGui::TreePop();
                    }

                    //Material texture descriptions
                    std::vector<TextureDesc> TextureDescs = {};

                    //Constant name checksums
                    std::vector<u32> ConstantNameChecksums = {};

                    //Constant values
                    std::vector<MaterialConstant> Constants = {};

                    ImGui::TreePop();
                }
            }
            ImGui::TreePop();
        }
        ImGui::PopStyleVar();

        ImGui::Separator();
        state->FontManager->FontL.Push();
        ImGui::Text(ICON_FA_IMAGE " Textures");
        state->FontManager->FontL.Pop();
        ImGui::Separator();

        gui::LabelAndValue("Diffuse:", DiffusePegPath.value_or("Not found"));
        gui::LabelAndValue("Specular:", SpecularPegPath.value_or("Not found"));
        gui::LabelAndValue("Normal:", NormalPegPath.value_or("Not found"));

        //Submesh data
        ImGui::Separator();
        state->FontManager->FontL.Push();
        ImGui::Text(ICON_FA_CUBES " Submeshes");
        state->FontManager->FontL.Pop();
        ImGui::Separator();

        //LOD level selector
        auto& mesh = Scene->Objects[0]->ObjectMesh;
        u32 lodLevel = mesh.GetLodLevel();
        u32 min = 0;
        u32 max = mesh.NumLods() - 1;
        bool lodChanged = ImGui::SliderScalar("LOD Level", ImGuiDataType_U32, &lodLevel, &min, &max, nullptr);
        ImGui::SameLine();
        gui::HelpMarker("Lower is higher quality.", state->FontManager->FontDefault.GetFont());
        if (lodChanged)
        {
            mesh.SetLodLevel(lodLevel);
            Scene->NeedsRedraw = true;
        }

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
            auto result = OpenFolder();
            if (!result)
                return;

            MeshExportPath = result.value();
        }

        //Disable mesh export button if export is disabled
        if (!meshExportEnabled_)
        {
            ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
        }
        if (ImGui::Button("Export"))
        {
            if (!std::filesystem::exists(MeshExportPath))
            {
                LOG_ERROR("Failed to export {} to obj. Output folder \"{}\" does not exist.", StaticMesh.Name, MeshExportPath);
            }
            else
            {
                string exportFolderPath = MeshExportPath + "\\";

                //Save mesh textures
                string diffuseFile = "";
                string specularFile = "";
                string normalFile = "";
                if (DiffuseName && DiffusePegPath)
                {
                    //Load peg
                    std::optional<PegFile10> peg = state->TextureSearchIndex->GetPegFromPath(DiffusePegPath.value());
                    defer(if (peg) peg.value().Cleanup());

                    //Write texture to file
                    if (peg && PegHelpers::ExportSingle(peg.value(), exportFolderPath, DiffuseName.value()))
                        diffuseFile = Path::GetFileNameNoExtension(DiffuseName.value()) + ".dds";
                }
                if (SpecularName && SpecularPegPath)
                {
                    //Load peg
                    std::optional<PegFile10> peg = state->TextureSearchIndex->GetPegFromPath(SpecularPegPath.value());
                    defer(if (peg) peg.value().Cleanup());

                    //Write texture to file
                    if (peg && PegHelpers::ExportSingle(peg.value(), exportFolderPath, SpecularName.value()))
                        specularFile = Path::GetFileNameNoExtension(SpecularName.value()) + ".dds";
                }
                if (NormalName && NormalPegPath)
                {
                    //Load peg
                    std::optional<PegFile10> peg = state->TextureSearchIndex->GetPegFromPath(NormalPegPath.value());
                    defer(if (peg) peg.value().Cleanup());

                    //Write texture to file
                    if (peg && PegHelpers::ExportSingle(peg.value(), exportFolderPath, NormalName.value()))
                        normalFile = Path::GetFileNameNoExtension(NormalName.value()) + ".dds";
                }

                //Read packfile holding the mesh
                Handle<Packfile3> packfile = InContainer ? state->PackfileVFS->GetContainer(ParentName, VppName) : state->PackfileVFS->GetPackfile(VppName);
                if (packfile)
                {
                    //Read mesh
                    string gpuFileName = RfgUtil::CpuFilenameToGpuFilename(Filename);
                    std::vector<u8> gpuFileBytes = packfile->ExtractSingleFile(gpuFileName, true).value();

                    //Export mesh as gltf file
                    BinaryReader gpuFileReader(gpuFileBytes);
                    StaticMesh.WriteToGltf(gpuFileReader, fmt::format("{}/{}.gltf", MeshExportPath, Path::GetFileNameNoExtension(Filename)), diffuseFile, specularFile, normalFile);
                }
                else
                {
                    LOG_ERROR("Failed to get packfile {}/{} for {} export.", VppName, ParentName, Filename);
                }
            }
        }
        if (!meshExportEnabled_)
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

    ImGui::PopStyleVar();
}

//Used to end mesh load task early if it was cancelled
#define TaskEarlyExitCheck() if (meshLoadTask_->Cancelled()) return;

void StaticMeshDocument::WorkerThread(Handle<Task> task, GuiState* state)
{
    WorkerStatusString = "Parsing header...";

    //Get gpu filename
    string gpuFileName = RfgUtil::CpuFilenameToGpuFilename(Filename);
    TaskEarlyExitCheck();

    //Read packfile holding the mesh
    Handle<Packfile3> packfile = InContainer ? state->PackfileVFS->GetContainer(ParentName, VppName) : state->PackfileVFS->GetPackfile(VppName);
    if (!packfile)
    {
        LOG_ERROR("Failed to get packfile {}/{} for {}", VppName, ParentName, Filename);
        Open = false;
        return;
    }

    //Read mesh data
    std::vector<u8> cpuFileBytes = packfile->ExtractSingleFile(Filename, true).value();
    std::vector<u8> gpuFileBytes = packfile->ExtractSingleFile(gpuFileName, true).value();

    //Read mesh header
    BinaryReader cpuFileReader(cpuFileBytes);
    BinaryReader gpuFileReader(gpuFileBytes);
    string ext = Path::GetExtension(Filename);
    if (ext == ".csmesh_pc")
        StaticMesh.Read(cpuFileReader, Filename, 0xC0FFEE11, 5); //Todo: Move signature + version into class or helper function.
    else if (ext == ".ccmesh_pc")
        StaticMesh.Read(cpuFileReader, Filename, 0xFAC351A9, 4);

    //Get material based on vertex format
    VertexFormat format = StaticMesh.MeshInfo.VertFormat;
    Log->info("Mesh vertex format: {}", to_string(format));

    //Two steps for each submesh: Get index/vertex buffers and find textures
    u32 numSteps = 2;
    f32 stepSize = 1.0f / (f32)numSteps;
    WorkerStatusString = "Loading mesh...";
    TaskEarlyExitCheck();

    //Read index and vertex buffers from gpu file
    auto maybeMeshData = StaticMesh.ReadMeshData(gpuFileReader);
    if (!maybeMeshData)
    {
        LOG_ERROR("Failed to read mesh data for static mesh document {}", Filename);
        Open = false;
        return;
    }

    //Load mesh and create render object from it
    MeshInstanceData meshData = maybeMeshData.value();
    Handle<RenderObject> renderObject = Scene->CreateRenderObject(to_string(format), Mesh{ Scene->d3d11Device_, meshData, StaticMesh.NumLods });

    //Set camera position to get a better view of the mesh
    {
        auto& submesh0 = StaticMesh.MeshInfo.Submeshes[0];
        Vec3 pos = submesh0.Bmax - submesh0.Bmin;
        Scene->Cam.SetPosition(pos.z, pos.y, pos.x); //x and z intentionally switched since that usually has a better result
        Scene->Cam.SetNearPlane(0.1f);
        Scene->Cam.Speed = 0.05f;
    }

    WorkerProgressFraction += stepSize;
    meshExportEnabled_ = true;

    bool foundDiffuse = false;
    bool foundSpecular = false;
    bool foundNormal = false;

    //Todo: Fully support RFGs material system. Currently just takes diffuse, normal, and specular textures
    //Remove duplicates
    std::vector<string> textures = StaticMesh.TextureNames;
    std::sort(textures.begin(), textures.end());
    textures.erase(std::unique(textures.begin(), textures.end()), textures.end());
    for (auto& texture : textures)
        texture = String::ToLower(texture);

    //Remove textures that don't fit the current material system of Nanoforge
    textures.erase(std::remove_if(textures.begin(), textures.end(),
        [](string& str)
        {
            return !(String::EndsWith(str, "_d.tga") || String::EndsWith(str, "_n.tga") || String::EndsWith(str, "_s.tga") ||
                     String::EndsWith(str, "_d_low.tga") || String::EndsWith(str, "_n_low.tga") || String::EndsWith(str, "_s_low.tga"));
        }), textures.end());

    //Load textures
    for (auto& textureName : textures)
    {
        //Check if the document was closed. If so, end worker thread early
        if (meshLoadTask_->Cancelled())
            return;

        string textureNameLower = String::ToLower(textureName);
        if (!foundDiffuse && String::Contains(textureNameLower, "_d"))
        {
            std::optional<Texture2D> texture = state->TextureSearchIndex->GetRenderTexture(textureName, Scene->d3d11Device_, true);
            if (texture)
            {
                Log->info("Found diffuse texture {} for {}", texture.value().Name, Filename);
                std::lock_guard<std::mutex> lock(state->Renderer->ContextMutex);
                renderObject->UseTextures = true;
                renderObject->Textures[0] = texture.value();
                DiffuseName = texture.value().Name;
                DiffusePegPath = state->TextureSearchIndex->GetTexturePegPath(texture.value().Name, true);
                foundDiffuse = true;
            }
            else
            {
                Log->warn("Failed to find diffuse texture {} for {}", textureName, Filename);
            }
        }
        else if (!foundNormal && String::Contains(textureNameLower, "_n"))
        {
            std::optional<Texture2D> texture = state->TextureSearchIndex->GetRenderTexture(textureName, Scene->d3d11Device_, true);
            if (texture)
            {
                Log->info("Found normal map {} for {}", texture.value().Name, Filename);

                std::lock_guard<std::mutex> lock(state->Renderer->ContextMutex);
                renderObject->UseTextures = true;
                renderObject->Textures[2] = texture.value();
                NormalName = texture.value().Name;
                NormalPegPath = state->TextureSearchIndex->GetTexturePegPath(texture.value().Name, true);
                foundNormal = true;
            }
            else
            {
                Log->warn("Failed to find normal map {} for {}", textureName, Filename);
            }
        }
        else if (!foundSpecular && String::Contains(textureNameLower, "_s"))
        {
            std::optional<Texture2D> texture = state->TextureSearchIndex->GetRenderTexture(textureName, Scene->d3d11Device_, true);
            if (texture)
            {
                Log->info("Found specular map {} for {}", texture.value().Name, Filename);

                std::lock_guard<std::mutex> lock(state->Renderer->ContextMutex);
                renderObject->UseTextures = true;
                renderObject->Textures[1] = texture.value();
                SpecularName = texture.value().Name;
                SpecularPegPath = state->TextureSearchIndex->GetTexturePegPath(texture.value().Name, true);
                foundSpecular = true;
            }
            else
            {
                Log->warn("Failed to find specular map {} for {}", textureName, Filename);
            }
        }
    }

    WorkerProgressFraction += stepSize;
    WorkerStatusString = "Done! " ICON_FA_CHECK;
    Log->info("Worker thread for {} finished.", Title);
}