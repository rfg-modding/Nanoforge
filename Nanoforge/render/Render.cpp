#include "Render.h"

std::unordered_map<string, Material> Render::materials_;

void Render::Init(ComPtr<ID3D11Device> d3d11Device, ComPtr<ID3D11DeviceContext> d3d11Context, Config* config)
{
    //RFG mesh formats
    materials_["Terrain"] = { d3d11Device, config, "Terrain.hlsl",
    {
        { "POSITION", 0,  DXGI_FORMAT_R16G16_SINT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0,  DXGI_FORMAT_R8G8B8A8_UNORM, 0, 4, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    }};
    materials_["TerrainLowLod"] = { d3d11Device, config, "TerrainLowLod.hlsl",
    {
        { "POSITION", 0,  DXGI_FORMAT_R16G16B16A16_SINT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    }};
    materials_["TerrainRoad"] = { d3d11Device, config, "TerrainRoad.hlsl",
    {
        { "POSITION", 0,  DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0,  DXGI_FORMAT_R8G8B8A8_UNORM, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TANGENT", 0,  DXGI_FORMAT_R8G8B8A8_UNORM, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0,  DXGI_FORMAT_R16G16_SINT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    }};
    materials_["TerrainStitch"] = { d3d11Device, config, "TerrainStitch.hlsl",
    {
        { "POSITION", 0,  DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0,  DXGI_FORMAT_R8G8B8A8_UNORM, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    }};
    materials_["Pixlit"] = { d3d11Device, config, "Pixlit.hlsl",
    {
        { "POSITION", 0,  DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0,  DXGI_FORMAT_R8G8B8A8_UNORM, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    }};
    materials_["Pixlit1Uv"] = { d3d11Device, config, "Pixlit1Uv.hlsl",
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0,  DXGI_FORMAT_R8G8B8A8_UNORM, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0,  DXGI_FORMAT_R16G16_SINT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    }};
    materials_["Pixlit1UvNmap"] = { d3d11Device, config, "Pixlit1UvNmap.hlsl",
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0,  DXGI_FORMAT_R8G8B8A8_UNORM, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TANGENT", 0,  DXGI_FORMAT_R8G8B8A8_UNORM, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0,  DXGI_FORMAT_R16G16_SINT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    }};
    materials_["Pixlit1UvNmapCa"] = { d3d11Device, config, "Pixlit1UvNmapCa.hlsl",
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0,  DXGI_FORMAT_R8G8B8A8_UNORM, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TANGENT", 0,  DXGI_FORMAT_R8G8B8A8_UNORM, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "BLENDWEIGHT", 0,  DXGI_FORMAT_R8G8B8A8_UNORM, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "BLENDINDEX", 0,  DXGI_FORMAT_R8G8B8A8_UINT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0,  DXGI_FORMAT_R16G16_SINT, 0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    }};
    materials_["Pixlit2UvNmap"] = { d3d11Device, config, "Pixlit2UvNmap.hlsl",
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0,  DXGI_FORMAT_R8G8B8A8_UNORM, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TANGENT", 0,  DXGI_FORMAT_R8G8B8A8_UNORM, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0,  DXGI_FORMAT_R16G16_SINT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 1,  DXGI_FORMAT_R16G16_SINT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    }};
    materials_["Pixlit3UvNmap"] = { d3d11Device, config, "Pixlit3UvNmap.hlsl",
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0,  DXGI_FORMAT_R8G8B8A8_UNORM, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TANGENT", 0,  DXGI_FORMAT_R8G8B8A8_UNORM, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0,  DXGI_FORMAT_R16G16_SINT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 1,  DXGI_FORMAT_R16G16_SINT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 2,  DXGI_FORMAT_R16G16_SINT, 0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    }};
    materials_["Pixlit3UvNmapCa"] = { d3d11Device, config, "Pixlit3UvNmapCa.hlsl",
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0,  DXGI_FORMAT_R8G8B8A8_UNORM, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TANGENT", 0,  DXGI_FORMAT_R8G8B8A8_UNORM, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "BLENDWEIGHT", 0,  DXGI_FORMAT_R8G8B8A8_UNORM, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "BLENDINDEX", 0,  DXGI_FORMAT_R8G8B8A8_UINT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0,  DXGI_FORMAT_R16G16_SINT, 0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 1,  DXGI_FORMAT_R16G16_SINT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 2,  DXGI_FORMAT_R16G16_SINT, 0, 36, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    }};

    //Primitive formats
    materials_["Linelist"] = { d3d11Device, config, "Linelist.hlsl",
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    }};
}

Material* Render::GetMaterial(const string& name)
{
    auto search = materials_.find(name);
    if (search != materials_.end())
        return &search->second;

    return {};
}

void Render::ReloadEditedShaders()
{
    for (auto& kv : materials_)
        kv.second.TryShaderReload();
}
