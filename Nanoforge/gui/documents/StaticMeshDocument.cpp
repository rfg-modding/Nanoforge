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

StaticMeshDocument::StaticMeshDocument(GuiState* state, string filename, string parentName, string vppName, bool inContainer)
    : Filename(filename), ParentName(parentName), VppName(vppName), InContainer(inContainer)
{
    state_ = state;

    //Create scene instance and store index
    Scene = state->Renderer->CreateScene();
    if (!Scene)
        THROW_EXCEPTION("Failed to create scene for static mesh document \"{}\"", filename);

    //Init scene and camera
    Scene->Cam.Init({ 7.5f, 15.0f, 12.0f }, 80.0f, { (f32)Scene->Width(), (f32)Scene->Height() }, 1.0f, 10000.0f);
    Scene->Cam.Speed = 0.25f;
    Scene->Cam.SprintSpeed = 0.4f;
    Scene->Cam.LookAt({ 0.0f, 0.0f, 0.0f });

    //Create worker thread to load terrain meshes in background
    meshLoadTask_ = Task::Create(fmt::format("Loading {}...", filename));
    TaskScheduler::QueueTask(meshLoadTask_, std::bind(&StaticMeshDocument::WorkerThread, this, meshLoadTask_, state));
}

StaticMeshDocument::~StaticMeshDocument()
{
    //Wait for worker thread to so we don't destroy resources it's using
    Open = false;
    meshLoadTask_->CancelAndWait();

    //Delete scene and free its resources
    state_->Renderer->DeleteScene(Scene);
    Scene = nullptr;
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
}

void StaticMeshDocument::Save(GuiState* state)
{

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
        ImGui::DragFloat3("Scale", (float*)&Scene->Objects[0]->Scale, 0.01, 1.0f, 100.0f);

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

        //Submesh data
        ImGui::Separator();
        state->FontManager->FontL.Push();
        ImGui::Text(ICON_FA_CUBES "Submeshes");
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
}

//Used to end mesh load task early if it was cancelled
#define TaskEarlyExitCheck() if (meshLoadTask_->Cancelled()) return;

void StaticMeshDocument::WorkerThread(Handle<Task> task, GuiState* state)
{
    WorkerStatusString = "Parsing header...";

    //Get gpu filename
    string gpuFileName = RfgUtil::CpuFilenameToGpuFilename(Filename);
    TaskEarlyExitCheck();

    //Get mesh data
    Packfile3* packfile = InContainer ? state->PackfileVFS->GetContainer(ParentName, VppName) : state->PackfileVFS->GetPackfile(VppName);
    packfile->ReadMetadata();
    std::span<u8> cpuFileBytes = packfile->ExtractSingleFile(Filename, true).value();
    std::span<u8> gpuFileBytes = packfile->ExtractSingleFile(gpuFileName, true).value();

    //Read mesh header
    BinaryReader cpuFileReader(cpuFileBytes);
    BinaryReader gpuFileReader(gpuFileBytes);
    string ext = Path::GetExtension(Filename);
    if (ext == ".csmesh_pc")
        StaticMesh.Read(cpuFileReader, Filename, 0xC0FFEE11, 5); //Todo: Move signature + version into class or helper function.
    else if (ext == ".ccmesh_pc")
        StaticMesh.Read(cpuFileReader, Filename, 0xFAC351A9, 4);

    Log->info("Mesh vertex format: {}", to_string(StaticMesh.MeshInfo.VertFormat));

    TaskEarlyExitCheck();

    //Get material based on vertex format
    WorkerStatusString = "Setting up scene...";
    VertexFormat format = StaticMesh.MeshInfo.VertFormat;

    //Two steps for each submesh: Get index/vertex buffers and find textures
    u32 numSteps = 2;
    f32 stepSize = 1.0f / (f32)numSteps;

    TaskEarlyExitCheck();

    WorkerStatusString = "Loading mesh...";

    //Read index and vertex buffers from gpu file
    auto maybeMeshData = StaticMesh.ReadMeshData(gpuFileReader);
    if (!maybeMeshData)
        THROW_EXCEPTION("Failed to read submesh mesh data for static mesh document.");

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
        {
            delete[] meshData.IndexBuffer.data();
            delete[] meshData.VertexBuffer.data();
            return;
        }

        string textureNameLower = String::ToLower(textureName);
        if (!foundDiffuse && String::Contains(textureNameLower, "_d"))
        {
            std::optional<Texture2D_Ext> texture = FindTexture(state, textureNameLower, true);
            if (texture)
            {
                Log->info("Found diffuse texture {} for {}", texture->Texture.Name, Filename);
                std::lock_guard<std::mutex> lock(state->Renderer->ContextMutex);
                renderObject->UseTextures = true;
                renderObject->Texture0 = texture.value().Texture;
                foundDiffuse = true;

                //Store path for exporting
                if (DiffuseMapPegPath == "")
                {
                    DiffuseMapPegPath = texture.value().CpuFilePath;
                    DiffuseTextureName = texture.value().Texture.Name;
                }
            }
            else
            {
                Log->warn("Failed to find diffuse texture {} for {}", textureNameLower, Filename);
            }
        }
        else if (!foundNormal && String::Contains(textureNameLower, "_n"))
        {
            std::optional<Texture2D_Ext> texture = FindTexture(state, textureNameLower, true);
            if (texture)
            {
                Log->info("Found normal map {} for {}", texture->Texture.Name, Filename);

                std::lock_guard<std::mutex> lock(state->Renderer->ContextMutex);
                renderObject->UseTextures = true;
                renderObject->Texture2 = texture.value().Texture;
                foundNormal = true;

                //Store path for exporting
                if (NormalMapPegPath == "")
                {
                    NormalMapPegPath = texture.value().CpuFilePath;
                    NormalTextureName = texture.value().Texture.Name;
                }
            }
            else
            {
                Log->warn("Failed to find normal map {} for {}", textureNameLower, Filename);
            }
        }
        else if (!foundSpecular && String::Contains(textureNameLower, "_s"))
        {
            std::optional<Texture2D_Ext> texture = FindTexture(state, textureNameLower, true);
            if (texture)
            {
                Log->info("Found specular map {} for {}", texture->Texture.Name, Filename);

                std::lock_guard<std::mutex> lock(state->Renderer->ContextMutex);
                renderObject->UseTextures = true;
                renderObject->Texture1 = texture.value().Texture;
                foundSpecular = true;

                //Store path for exporting
                if (SpecularMapPegPath == "")
                {
                    SpecularMapPegPath = texture.value().CpuFilePath;
                    SpecularTextureName = texture.value().Texture.Name;
                }
            }
            else
            {
                Log->warn("Failed to find specular map {} for {}", textureNameLower, Filename);
            }
        }
    }

    WorkerProgressFraction += stepSize;

    if (InContainer)
        delete packfile;

    delete[] cpuFileBytes.data();
    delete[] gpuFileBytes.data();

    //Clear mesh data
    delete[] meshData.IndexBuffer.data();
    delete[] meshData.VertexBuffer.data();

    WorkerStatusString = "Done! " ICON_FA_CHECK;
    Log->info("Worker thread for {} finished.", Title);
}

//Used by texture search functions to stop the search early if mesh load task is cancelled
#define TaskEarlyExitCheck2() if (meshLoadTask_->Cancelled()) return {};

//Finds a texture and creates a directx texture resource from it. textureName is the textureName of a texture inside a cpeg/cvbm. So for example, sledgehammer_high_n.tga, which is in sledgehammer_high.cpeg_pc
//Will try to find a high res version of the texture first if lookForHighResVariant is true.
//Will return a default texture if the target isn't found.
std::optional<Texture2D_Ext> StaticMeshDocument::FindTexture(GuiState* state, const string& name, bool lookForHighResVariant)
{
    PROFILER_FUNCTION();

    //Look for high res variant if requested and string fits high res search requirements
    if (lookForHighResVariant && String::Contains(name, "_low_"))
    {
        //Replace _low_ with _. This is the naming scheme I've seen many high res variants follow
        string highResName = String::Replace(name, "_low_", "_");
        auto texture = GetTexture(state, highResName, false);

        //Return high res variant if it was found
        if (texture)
            return texture;
    }
    TaskEarlyExitCheck2();

    //Else look for the specified texture
    return GetTexture(state, name, true);
}

std::optional<Texture2D_Ext> StaticMeshDocument::GetTexture(GuiState* state, const string& textureName, bool useLastResortSearches)
{
    PROFILER_FUNCTION();

    //Search parent vpp
    TaskEarlyExitCheck2();
    WorkerStatusString = fmt::format("Searching for textures in {}...", VppName);
    auto parentSearchResult = GetTextureFromPackfile(state, state->PackfileVFS->GetPackfile(VppName), textureName);
    if (parentSearchResult)
        return parentSearchResult;

    if (useLastResortSearches)
    {
        if (String::Contains(textureName, "rayen_haira"))
        {
            WorkerStatusString = fmt::format("Searching for textures in dlcp01_items.vpp_pc...");
            auto parentSearchResult = GetTextureFromPackfile(state, state->PackfileVFS->GetPackfile("dlcp01_items.vpp_pc"), textureName);
            if (parentSearchResult)
                return parentSearchResult;
        }
        if (String::Contains(textureName, "edf_mpc_biggun"))
        {
            WorkerStatusString = fmt::format("Searching for textures in terr01_l1.vpp_pc...");
            auto parentSearchResult = GetTextureFromPackfile(state, state->PackfileVFS->GetPackfile("terr01_l1.vpp_pc"), textureName);
            if (parentSearchResult)
                return parentSearchResult;
        }

        //Search some other vpps that commonly have mesh textures
        static const std::vector<string> searchList =
        {
            "dlc01_precache.vpp_pc",
            "items.vpp_pc",
            "dlcp01_items.vpp_pc",
            "terr01_l1.vpp_pc",
            "terr01_l0.vpp_pc",
            "vehicles_r.vpp_pc"
        };
        for (auto& packfile : searchList)
        {
            WorkerStatusString = fmt::format("Searching for textures in {}...", packfile);
            auto result = GetTextureFromPackfile(state, state->PackfileVFS->GetPackfile(packfile), textureName);
            if (result)
                return result;
        }

        //As a last resort search every file
        for (Packfile3& packfile : state->PackfileVFS->packfiles_)
        {
            WorkerStatusString = fmt::format("Searching all packfiles for textures as a last resort. This could take a while...");
            auto result = GetTextureFromPackfile(state, &packfile, textureName);
            if (result)
                return result;
        }
    }

    return {};
}

//Tries to find a cpeg with a subtexture with the provided name and create a Texture2D from it. Searches all cpeg/cvbm files in packfile. First checks pegs then searches in str2s
std::optional<Texture2D_Ext> StaticMeshDocument::GetTextureFromPackfile(GuiState* state, Packfile3* packfile, const string& textureName)
{
    PROFILER_FUNCTION();

    if (!packfile)
        return {};

    //First search top level cpeg/cvbm files
    for (u32 i = 0; i < packfile->Entries.size(); i++)
    {
        TaskEarlyExitCheck2();

        Packfile3Entry& entry = packfile->Entries[i];
        const char* entryName = packfile->EntryNames[i];
        string ext = Path::GetExtension(entryName);

        //Try to get texture from each cpeg/cvbm
        if (ext == ".cpeg_pc" || ext == ".cvbm_pc")
        {
            auto texture = GetTextureFromPeg(state, packfile->Name(), packfile->Name(), entryName, textureName, false);
            if (texture)
                return texture;
        }
    }

    //Then search inside each str2
    for (u32 i = 0; i < packfile->Entries.size(); i++)
    {
        //Check if the document was closed. If so, end worker thread early
        TaskEarlyExitCheck2();

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
            for (u32 i = 0; i < container.Entries.size(); i++)
            {
                TaskEarlyExitCheck2();

                Packfile3Entry& entry = container.Entries[i];
                const char* entryName = container.EntryNames[i];
                string ext = Path::GetExtension(entryName);

                //Try to get texture from each cpeg/cvbm
                if (ext == ".cpeg_pc" || ext == ".cvbm_pc")
                {
                    auto texture = GetTextureFromPeg(state, packfile->Name(), container.Name(), entryName, textureName, true);
                    if (texture)
                        return texture;
                }
            }
        }
    }

    //If didn't find texture at this point then failed. Return empty
    return {};
}

//Tries to open a cpeg/cvbm pegName in the provided packfile and create a Texture2D from a sub-texture with the name textureName
std::optional<Texture2D_Ext> StaticMeshDocument::GetTextureFromPeg(GuiState* state,
    const string& vppName, //Name of the vpp_pc the peg or it's parent is in
    const string& parentName, //If inContainer == true this is the name of str2_pc the peg is in. Else it is the same as VppName
    const string& pegName, //Name of the cpeg_pc or cvbm_pc file that holds the target texture
    const string& textureName, //Name of the target texture. E.g. "sledgehammer_d.tga"
    bool inContainer) //Whether or not the cpeg_pc/cvbm_pc is in a str2_pc file or not
{
    PROFILER_FUNCTION();

    //Get gpu filename
    string gpuFilename = RfgUtil::CpuFilenameToGpuFilename(pegName);

    Packfile3* packfile = inContainer ? state->PackfileVFS->GetContainer(parentName, vppName) : state->PackfileVFS->GetPackfile(vppName);
    if(inContainer)
        packfile->ReadMetadata();
    std::span<u8> cpuFileBytes = packfile->ExtractSingleFile(pegName, true).value();
    std::span<u8> gpuFileBytes = packfile->ExtractSingleFile(gpuFilename, true).value();

    BinaryReader cpuFileReader(cpuFileBytes);
    BinaryReader gpuFileReader(gpuFileBytes);

    //Parse peg file
    std::optional<Texture2D_Ext> out = {};
    PegFile10 peg;
    peg.Read(cpuFileReader);

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
            bool srgb = (peg.Flags & 512) != 0;
            DXGI_FORMAT dxgiFormat = PegHelpers::PegFormatToDxgiFormat(entry.BitmapFormat, srgb);
            D3D11_SUBRESOURCE_DATA textureSubresourceData;
            textureSubresourceData.pSysMem = textureData.data();
            textureSubresourceData.SysMemSlicePitch = 0;
            textureSubresourceData.SysMemPitch = PegHelpers::CalcRowPitch(dxgiFormat, entry.Width);

            state->Renderer->ContextMutex.lock(); //Lock ID3D11DeviceContext mutex. Only one thread allowed to access it at once
            texture2d.Create(Scene->d3d11Device_, entry.Width, entry.Height, dxgiFormat, D3D11_BIND_SHADER_RESOURCE, &textureSubresourceData);
            texture2d.CreateSampler(); //Create sampler for shader use
            state->Renderer->ContextMutex.unlock();

            out = Texture2D_Ext{ .Texture = texture2d, .CpuFilePath = pegName };
            break;
        }
    }

    if (inContainer)
        delete packfile;

    delete[] cpuFileBytes.data();
    delete[] gpuFileBytes.data();

    //Release allocated memory and return output
    peg.Cleanup();
    return out;
}