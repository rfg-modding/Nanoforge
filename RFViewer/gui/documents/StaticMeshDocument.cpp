#include "StaticMeshDocument.h"
#include "render/backend/DX11Renderer.h"
#include "util/RfgUtil.h"
#include "common/filesystem/Path.h"
#include "common/string/String.h"
#include "RfgTools++/formats/textures/PegFile10.h"
#include "Log.h"
#include "gui/documents/PegHelpers.h"
#include <optional>

#ifdef DEBUG_BUILD
    const string shaderFolderPath_ = "C:/Users/moneyl/source/repos/Project28/Assets/shaders/";
#else
    const string shaderFolderPath_ = "./Assets/shaders/";
#endif

void StaticMeshDocument_WorkerThread(GuiState* state, std::shared_ptr<Document> doc);
//Finds a texture and creates a directx texture resource from it. textureName is the textureName of a texture inside a cpeg/cvbm. So for example, sledgehammer_high_n.tga, which is in sledgehammer_high.cpeg_pc
//Will try to find a high res version of the texture first if lookForHighResVariant is true.
//Will return a default texture if the target isn't found.
std::optional<Texture2D> FindTexture(GuiState* state, std::shared_ptr<Document> doc, const string& name, bool lookForHighResVariant = false);

void StaticMeshDocument_Init(GuiState* state, std::shared_ptr<Document> doc)
{
    StaticMeshDocumentData* data = (StaticMeshDocumentData*)doc->Data;

    //Create scene instance and store index
    state->Renderer->CreateScene();
    data->Scene = state->Renderer->Scenes.back();

    //Init scene and camera
    data->Scene->Cam.Init({ 7.5f, 15.0f, 12.0f }, 80.0f, { (f32)data->Scene->Width(), (f32)data->Scene->Height() }, 1.0f, 10000.0f);
    data->Scene->Cam.Speed = 0.25f;
    data->Scene->Cam.SprintSpeed = 0.4f;
    data->Scene->Cam.LookAt({ 0.0f, 0.0f, 0.0f });

    //Create worker thread to load terrain meshes in background
    data->WorkerFuture = std::async(std::launch::async, &StaticMeshDocument_WorkerThread, state, doc);
}

void StaticMeshDocument_DrawOverlayButtons(GuiState* state, std::shared_ptr<Document> doc);

void StaticMeshDocument_Update(GuiState* state, std::shared_ptr<Document> doc)
{
    StaticMeshDocumentData* data = (StaticMeshDocumentData*)doc->Data;
    if (!ImGui::Begin(doc->Title.c_str(), &doc->Open))
    {
        ImGui::End();
        return;
    }

    //Camera only handles input if window is focused
    data->Scene->Cam.InputActive = ImGui::IsWindowFocused();

    ImVec2 contentAreaSize;
    contentAreaSize.x = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;
    contentAreaSize.y = ImGui::GetWindowContentRegionMax().y - ImGui::GetWindowContentRegionMin().y;
    data->Scene->HandleResize(contentAreaSize.x, contentAreaSize.y);

    //Store initial position so we can draw buttons over the scene texture after drawing it
    ImVec2 initialPos = ImGui::GetCursorPos();

    //Render scene texture
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(data->Scene->ClearColor.x, data->Scene->ClearColor.y, data->Scene->ClearColor.z, data->Scene->ClearColor.w));
    ImGui::Image(data->Scene->GetView(), ImVec2(static_cast<f32>(data->Scene->Width()), static_cast<f32>(data->Scene->Height())));
    ImGui::PopStyleColor();

    //Set cursor pos to top left corner to draw buttons over scene texture
    ImVec2 adjustedPos = initialPos;
    adjustedPos.x += 10.0f;
    adjustedPos.y += 10.0f;
    ImGui::SetCursorPos(adjustedPos);

    StaticMeshDocument_DrawOverlayButtons(state, doc);

    ImGui::End();
}

void StaticMeshDocument_DrawOverlayButtons(GuiState* state, std::shared_ptr<Document> doc)
{
    StaticMeshDocumentData* data = (StaticMeshDocumentData*)doc->Data;

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

        f32 fov = data->Scene->Cam.GetFov();
        f32 nearPlane = data->Scene->Cam.GetNearPlane();
        f32 farPlane = data->Scene->Cam.GetFarPlane();
        f32 lookSensitivity = data->Scene->Cam.GetLookSensitivity();

        if (ImGui::Button("0.1")) data->Scene->Cam.Speed = 0.1f;
        ImGui::SameLine();
        if (ImGui::Button("1.0")) data->Scene->Cam.Speed = 1.0f;
        ImGui::SameLine();
        if (ImGui::Button("10.0")) data->Scene->Cam.Speed = 10.0f;
        ImGui::SameLine();
        if (ImGui::Button("25.0")) data->Scene->Cam.Speed = 25.0f;
        ImGui::SameLine();
        if (ImGui::Button("50.0")) data->Scene->Cam.Speed = 50.0f;
        ImGui::SameLine();
        if (ImGui::Button("100.0")) data->Scene->Cam.Speed = 100.0f;

        ImGui::InputFloat("Speed", &data->Scene->Cam.Speed);
        ImGui::InputFloat("Sprint speed", &data->Scene->Cam.SprintSpeed);
        ImGui::Separator();

        if (ImGui::InputFloat("Fov", &fov))
            data->Scene->Cam.SetFov(fov);
        if (ImGui::InputFloat("Near plane", &nearPlane))
            data->Scene->Cam.SetNearPlane(nearPlane);
        if (ImGui::InputFloat("Far plane", &farPlane))
            data->Scene->Cam.SetFarPlane(farPlane);
        if (ImGui::InputFloat("Look sensitivity", &lookSensitivity))
            data->Scene->Cam.SetLookSensitivity(lookSensitivity);

        ImGui::Separator();
        if (ImGui::InputFloat3("Position", (float*)&data->Scene->Cam.camPosition))
        {
            data->Scene->Cam.UpdateViewMatrix();
        }

        gui::LabelAndValue("Pitch:", std::to_string(data->Scene->Cam.GetPitch()));
        gui::LabelAndValue("Yaw:", std::to_string(data->Scene->Cam.GetYaw()));

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
            data->Scene->Objects[0].Scale.x = tempScale;
            data->Scene->Objects[0].Scale.y = tempScale;
            data->Scene->Objects[0].Scale.z = tempScale;
        }
        ImGui::DragFloat3("Scale", (float*)&data->Scene->Objects[0].Scale, 0.01, 1.0f, 100.0f);

        ImGui::ColorEdit3("Diffuse", reinterpret_cast<f32*>(&data->Scene->perFrameStagingBuffer_.DiffuseColor));
        ImGui::SliderFloat("Diffuse intensity", &data->Scene->perFrameStagingBuffer_.DiffuseIntensity, 0.0f, 1.0f);

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

        gui::LabelAndValue("Header size:", std::to_string(data->StaticMesh.CpuDataSize) + " bytes");
        gui::LabelAndValue("Data size:", std::to_string(data->StaticMesh.GpuDataSize) + " bytes");
        gui::LabelAndValue("Num LODs:", std::to_string(data->StaticMesh.NumLods));
        gui::LabelAndValue("Num submeshes:", std::to_string(data->StaticMesh.NumSubmeshes));
        gui::LabelAndValue("Num materials:", std::to_string(data->StaticMesh.Header.NumMaterials));
        gui::LabelAndValue("Num vertices:", std::to_string(data->StaticMesh.VertexBufferConfig.NumVerts));
        gui::LabelAndValue("Num indices:", std::to_string(data->StaticMesh.IndexBufferConfig.NumIndices));
        gui::LabelAndValue("Vertex format:", to_string(data->StaticMesh.VertexBufferConfig.Format));
        gui::LabelAndValue("Vertex size:", std::to_string(data->StaticMesh.VertexBufferConfig.VertexStride0));
        gui::LabelAndValue("Index size:", std::to_string(data->StaticMesh.IndexBufferConfig.IndexSize));

        //Submesh data
        ImGui::Separator();
        state->FontManager->FontL.Push();
        ImGui::Text(ICON_FA_CUBES "Submeshes");
        state->FontManager->FontL.Pop();
        ImGui::Separator();

        if (ImGui::BeginChild("##MeshInfoSubmeshesList", ImVec2(200.0f, 150.0f)))
        {
            ImGui::Columns(2);
            ImGui::SetColumnWidth(0, 100.0f);
            ImGui::SetColumnWidth(1, 100.0f);
            for (u32 i = 0; i < data->StaticMesh.SubMeshes.size(); i++)
            {
                SubmeshData& submesh = data->StaticMesh.SubMeshes[i];
                if (i >= data->RenderObjectIndices.size()) //Skip submeshes that aren't fully loaded yet
                    continue;

                RenderObject& renderObj = data->Scene->Objects[data->RenderObjectIndices[i]];
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
}

//Tries to open a cpeg/cvbm pegName in the provided packfile and create a Texture2D from a sub-texture with the name textureName
std::optional<Texture2D> GetTextureFromPeg(GuiState* state, std::shared_ptr<Document> doc, Packfile3* parent, const string& pegName, const string& textureName)
{
    StaticMeshDocumentData* data = (StaticMeshDocumentData*)doc->Data;

    //Get gpu filename
    string gpuFilename = RfgUtil::CpuFilenameToGpuFilename(pegName);

    //Get bytes for cpu file and gpu file
    auto cpuFileBytes = parent->ExtractSingleFile(pegName, true);
    if (!cpuFileBytes)
        return {};
    auto gpuFileBytes = parent->ExtractSingleFile(gpuFilename, true);
    if (!gpuFileBytes)
    {
        //Must release alllocated cpuFileBytes
        delete[] cpuFileBytes.value().data();
        return {};
    }

    //Parse peg file
    std::optional<Texture2D> out = {};
    BinaryReader cpuFileReader(cpuFileBytes.value());
    BinaryReader gpuFileReader(gpuFileBytes.value());
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
            DXGI_FORMAT dxgiFormat = PegHelpers::PegFormatToDxgiFormat(entry.BitmapFormat);
            D3D11_SUBRESOURCE_DATA textureSubresourceData;
            textureSubresourceData.pSysMem = textureData.data();
            textureSubresourceData.SysMemSlicePitch = 0;
            textureSubresourceData.SysMemPitch = PegHelpers::CalcRowPitch(dxgiFormat, entry.Width, entry.Height);
            state->Renderer->ContextMutex.lock(); //Lock ID3D11DeviceContext mutex. Only one thread allowed to access it at once
            texture2d.Create(data->Scene->d3d11Device_, entry.Width, entry.Height, dxgiFormat, D3D11_BIND_SHADER_RESOURCE, &textureSubresourceData);
            texture2d.CreateShaderResourceView(); //Need shader resource view to use it in shader
            texture2d.CreateSampler(); //Need sampler too
            state->Renderer->ContextMutex.unlock();

            out = texture2d;
        }
    }

    //Release allocated memory and return output
    peg.Cleanup();
    delete[] cpuFileBytes.value().data();
    delete[] gpuFileBytes.value().data();
    return out;
}

//Tries to find a cpeg with a subtexture with the provided name and create a Texture2D from it. Searches all cpeg/cvbm files in packfile. First checks pegs then searches in str2s
std::optional<Texture2D> GetTextureFromPackfile(GuiState* state, std::shared_ptr<Document> doc, Packfile3* packfile, const string& textureName, bool isStr2 = false)
{
    StaticMeshDocumentData* data = (StaticMeshDocumentData*)doc->Data;

    //First search top level cpeg/cvbm files
    for (u32 i = 0; i < packfile->Entries.size(); i++)
    {
        Packfile3Entry& entry = packfile->Entries[i];
        const char* entryName = packfile->EntryNames[i];
        string ext = Path::GetExtension(entryName);

        //Try to get texture from each cpeg/cvbm
        if (ext == ".cpeg_pc" || ext == ".cvbm_pc")
        {
            auto texture = GetTextureFromPeg(state, doc, packfile, entryName, textureName);
            if (texture)
                return texture;
        }
    }

    //Then search inside each str2 if this packfile isn't a str2
    if (!isStr2)
    {
        for (u32 i = 0; i < packfile->Entries.size(); i++)
        {
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
                container.ReadMetadata();
                auto texture = GetTextureFromPackfile(state, doc, &container, textureName, true);
                if (texture)
                    return texture;
            }
        }
    }

    //If didn't find texture at this point then failed. Return empty
    return {};
}

std::optional<Texture2D> GetTexture(GuiState* state, std::shared_ptr<Document> doc, const string& textureName)
{
    StaticMeshDocumentData* data = (StaticMeshDocumentData*)doc->Data;

    //First search current str2_pc if the mesh is inside one
    if (data->InContainer)
    {
        Packfile3* container = state->PackfileVFS->GetContainer(data->ParentName, data->VppName);
        if (container)
        {
            auto texture = GetTextureFromPackfile(state, doc, container, textureName, true);
            delete container; //Delete container since they're loaded on demand

            //Return texture if it was found
            if (texture)
                return texture;
        }
    }

    //Then search parent vpp
    Packfile3* parentPackfile = state->PackfileVFS->GetPackfile(data->VppName);
    if (!parentPackfile)
        return {};

    return GetTextureFromPackfile(state, doc, parentPackfile, textureName); //Return regardless here since it's our last search option
}

std::optional<Texture2D> FindTexture(GuiState* state, std::shared_ptr<Document> doc, const string& name, bool lookForHighResVariant)
{
    //Look for high res variant if requested and string fits high res search requirements
    if (lookForHighResVariant && String::Contains(name, "_low_"))
    {
        //Replace _low_ with _. This is the naming scheme I've seen many high res variants follow
        string highResName = String::Replace(name, "_low_", "_");
        auto texture = GetTexture(state, doc, highResName);
        
        //Return high res variant if it was found
        if (texture)
        {
            Log->info("Found high res variant of {}: {}", name, highResName);
            return texture;
        }
    }

    //Else look for the specified texture
    return GetTexture(state, doc, name);
}

void StaticMeshDocument_OnClose(GuiState* state, std::shared_ptr<Document> doc)
{
    StaticMeshDocumentData* data = (StaticMeshDocumentData*)doc->Data;

    //Delete scene and free its resources
    state->Renderer->DeleteScene(data->Scene);

    //Free document data
    delete data;
}

void StaticMeshDocument_WorkerThread(GuiState* state, std::shared_ptr<Document> doc)
{
    StaticMeshDocumentData* data = (StaticMeshDocumentData*)doc->Data;

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
    string ext = Path::GetExtension(data->Filename);
    //Todo: Move signature + version into class or helper function. Users of StaticMesh::Read shouldn't need to know these to use it
    if (ext == ".csmesh_pc")
        data->StaticMesh.Read(cpuFileReader, data->Filename, 0xC0FFEE11, 5);
    else if (ext == ".ccmesh_pc")
        data->StaticMesh.Read(cpuFileReader, data->Filename, 0xFAC351A9, 4);

    Log->info("Mesh vertex format: {}", to_string(data->StaticMesh.VertexBufferConfig.Format));

    //Todo: Put this in renderer / RenderObject code somewhere so it can be reused by other mesh code
    //Vary input and shader based on vertex format
    VertexFormat format = data->StaticMesh.VertexBufferConfig.Format;
    state->Renderer->ContextMutex.lock(); //Lock ID3D11DeviceContext mutex. Only one thread allowed to access it at once
    data->Scene->SetShader(shaderFolderPath_ + to_string(format) + ".fx");
    if (format == VertexFormat::Pixlit1Uv)
    {
        data->Scene->SetVertexLayout
        ({
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0,  DXGI_FORMAT_R8G8B8A8_UNORM, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0,  DXGI_FORMAT_R16G16_SINT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            });
    }
    else if (format == VertexFormat::Pixlit1UvNmap)
    {
        data->Scene->SetVertexLayout
        ({
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0,  DXGI_FORMAT_R8G8B8A8_UNORM, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TANGENT", 0,  DXGI_FORMAT_R8G8B8A8_UNORM, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0,  DXGI_FORMAT_R16G16_SINT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            });
    }
    else if (format == VertexFormat::Pixlit1UvNmapCa)
    {
        data->Scene->SetVertexLayout
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
        data->Scene->SetVertexLayout
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
        data->Scene->SetVertexLayout
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
        data->Scene->SetVertexLayout
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
    bool hideLowLodMeshes = data->StaticMesh.NumLods == data->StaticMesh.NumSubmeshes;

    //Track list of textures found and not found to avoid repeat searches
    std::unordered_map<string, Texture2D> foundTextures = {};
    std::unordered_map<string, int> missingTextures = {};

    for (u32 i = 0; i < data->StaticMesh.SubMeshes.size(); i++)
    {
        //Read index and vertex buffers from gpu file
        auto maybeMeshData = data->StaticMesh.ReadSubmeshData(gpuFileReader, i);
        if (!maybeMeshData)
            THROW_EXCEPTION("Failed to get mesh data for static mesh doc in StaticMesh::ReadSubmeshData()");

        state->Renderer->ContextMutex.lock();
        MeshInstanceData meshData = maybeMeshData.value();
        auto& renderObject = data->Scene->Objects.emplace_back();
        data->RenderObjectIndices.push_back(data->Scene->Objects.size() - 1);
        Mesh mesh;
        mesh.Create(data->Scene->d3d11Device_, data->Scene->d3d11Context_, meshData.VertexBuffer, meshData.IndexBuffer,
            data->StaticMesh.VertexBufferConfig.NumVerts, DXGI_FORMAT_R16_UINT, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        renderObject.Create(mesh, Vec3{ 0.0f, 0.0f, 0.0f });
        renderObject.SetScale(25.0f);
        if (hideLowLodMeshes && i > 0)
            renderObject.Visible = false;
        state->Renderer->ContextMutex.unlock();

        //Todo: Better handle materials. This works okay but doesn't always properly apply textures to meshes with multiple submeshes with different materials
        bool foundDiffuse = false;
        bool foundSpecular = false;
        bool foundNormal = false;
        for (auto& textureName : data->StaticMesh.TextureNames)
        {
            string textureNameLower = String::ToLower(textureName);
            bool missing = missingTextures.find(textureNameLower) != missingTextures.end();
            if (missing) //Skip textures that weren't found in previous searches
                continue;

            if (!foundDiffuse && String::Contains(textureNameLower, "_d"))
            {
                std::optional<Texture2D> texture = {};
                bool notInCache = foundTextures.find(textureNameLower) == foundTextures.end();
                if (notInCache)
                    texture = FindTexture(state, doc, textureNameLower, true);
                else
                    texture = foundTextures[textureNameLower];

                if (texture)
                {
                    if (notInCache)
                        Log->info("Found diffuse texture {} for {}", textureNameLower, data->Filename);
                    else
                        Log->info("Using cached copy of {} for {}", textureNameLower, data->Filename);

                    std::lock_guard<std::mutex> lock(state->Renderer->ContextMutex);
                    renderObject.UseTextures = true;
                    renderObject.DiffuseTexture = texture.value();
                    foundDiffuse = true;
                    if (foundTextures.find(textureNameLower) == foundTextures.end())
                        foundTextures[textureNameLower] = texture.value();
                }
                else
                {
                    missingTextures[textureNameLower] = 0;
                    Log->warn("Failed to find diffuse texture {} for {}", textureNameLower, data->Filename);
                }
            }
            else if (!foundNormal && String::Contains(textureNameLower, "_n"))
            {
                std::optional<Texture2D> texture = {};
                bool notInCache = foundTextures.find(textureNameLower) == foundTextures.end();
                if (notInCache)
                    texture = FindTexture(state, doc, textureNameLower, true);
                else
                    texture = foundTextures[textureNameLower];
                if (texture)
                {
                    if (notInCache)
                        Log->info("Found normal map {} for {}", textureNameLower, data->Filename);
                    else
                        Log->info("Using cached copy of {} for {}", textureNameLower, data->Filename);

                    std::lock_guard<std::mutex> lock(state->Renderer->ContextMutex);
                    renderObject.UseTextures = true;
                    renderObject.NormalTexture = texture.value();
                    foundNormal = true;
                    if (foundTextures.find(textureNameLower) == foundTextures.end())
                        foundTextures[textureNameLower] = texture.value();
                }
                else
                {
                    missingTextures[textureNameLower] = 0;
                    Log->warn("Failed to find normal map {} for {}", textureNameLower, data->Filename);
                }
            }
            else if (!foundSpecular && String::Contains(textureNameLower, "_s"))
            {
                std::optional<Texture2D> texture = {};
                bool notInCache = foundTextures.find(textureNameLower) == foundTextures.end();
                if (notInCache)
                    texture = FindTexture(state, doc, textureNameLower, true);
                else
                    texture = foundTextures[textureNameLower];
                if (texture)
                {
                    if (notInCache)
                        Log->info("Found specular map {} for {}", textureNameLower, data->Filename);
                    else
                        Log->info("Using cached copy of {} for {}", textureNameLower, data->Filename);

                    std::lock_guard<std::mutex> lock(state->Renderer->ContextMutex);
                    renderObject.UseTextures = true;
                    renderObject.SpecularTexture = texture.value();
                    foundSpecular = true;
                    if (foundTextures.find(textureNameLower) == foundTextures.end())
                        foundTextures[textureNameLower] = texture.value();
                }
                else
                {
                    missingTextures[textureNameLower] = 0;
                    Log->warn("Failed to find specular map {} for {}", textureNameLower, data->Filename);
                }
            }
        }

        //Clear mesh data
        delete[] meshData.IndexBuffer.data();
        delete[] meshData.VertexBuffer.data();
    }

    Log->info("Worker thread for {} finished.", doc->Title);
}