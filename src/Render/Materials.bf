using Nanoforge.Render.Resources;
using System.Collections;
using Direct3D;
using Common;
using System;
using Nanoforge.Misc;

namespace Nanoforge.Render
{
	public static class RenderMaterials
	{
        private static Dictionary<StringView, Material> _materials = new .() ~DeleteDictionaryAndValues!(_);

        public static void Init(ID3D11Device* device, ID3D11DeviceContext* context)
        {
            //RFG mesh formats
            CreateMaterial(device, "Terrain", "Terrain.hlsl",
			    D3D11_INPUT_ELEMENT_DESC[]
				(
                    .("POSITION", 0, .R16G16_SINT,    0, 0, .VERTEX_DATA, 0),
                    .("NORMAL",   0, .R8G8B8A8_UNORM, 0, 4, .VERTEX_DATA, 0),
			    ), false);

            CreateMaterial(device, "TerrainLowLod", "TerrainLowLod.hlsl",
	            D3D11_INPUT_ELEMENT_DESC[]
	            (
	                .("POSITION", 0, .R16G16B16A16_SINT, 0, 0, .VERTEX_DATA, 0),
	            ), false);

            CreateMaterial(device, "TerrainRoad", "TerrainRoad.hlsl",
	            D3D11_INPUT_ELEMENT_DESC[]
	            (
	                .("POSITION", 0, .R32G32B32_FLOAT, 0, 0,  .VERTEX_DATA, 0),
                    .("NORMAL",   0, .R8G8B8A8_UNORM,  0, 12, .VERTEX_DATA, 0),
                    .("TANGENT",  0, .R8G8B8A8_UNORM,  0, 16, .VERTEX_DATA, 0),
	                .("TEXCOORD", 0, .R16G16_SINT,     0, 20, .VERTEX_DATA, 0),
	            ), false);

            CreateMaterial(device, "TerrainStitch", "TerrainStitch.hlsl",
	            D3D11_INPUT_ELEMENT_DESC[]
	            (
	                .("POSITION", 0, .R32G32B32_FLOAT, 0, 0,  .VERTEX_DATA, 0),
	                .("NORMAL",   0, .R8G8B8A8_UNORM,  0, 12, .VERTEX_DATA, 0),
	            ), false);

            CreateMaterial(device, "Pixlit", "Pixlit.hlsl",
	            D3D11_INPUT_ELEMENT_DESC[]
	            (
	                .("POSITION", 0, .R32G32B32_FLOAT, 0, 0,  .VERTEX_DATA, 0),
	                .("NORMAL",   0, .R8G8B8A8_UNORM,  0, 12, .VERTEX_DATA, 0),
	            ), false);

            CreateMaterial(device, "Pixlit1Uv", "Pixlit1Uv.hlsl",
	            D3D11_INPUT_ELEMENT_DESC[]
	            (
	                .("POSITION", 0, .R32G32B32_FLOAT, 0, 0,  .VERTEX_DATA, 0),
                    .("NORMAL",   0, .R8G8B8A8_UNORM,  0, 12, .VERTEX_DATA, 0),
	                .("TEXCOORD", 0, .R16G16_SINT,     0, 16, .VERTEX_DATA, 0),
	            ), false);

            CreateMaterial(device, "PixlitNmap", "Pixlit1UvNmap.hlsl",
	            D3D11_INPUT_ELEMENT_DESC[]
	            (
	                .("POSITION", 0, .R32G32B32_FLOAT, 0, 0,  .VERTEX_DATA, 0),
	                .("NORMAL",   0, .R8G8B8A8_UNORM,  0, 12, .VERTEX_DATA, 0),
                    .("TANGENT",  0, .R8G8B8A8_UNORM,  0, 16, .VERTEX_DATA, 0),
	                .("TEXCOORD", 0, .R16G16_SINT,     0, 20, .VERTEX_DATA, 0),
	            ), false);

            CreateMaterial(device, "Pixlit1UvNmap", "Pixlit1UvNmap.hlsl",
	            D3D11_INPUT_ELEMENT_DESC[]
	            (
	                .("POSITION", 0, .R32G32B32_FLOAT, 0, 0,  .VERTEX_DATA, 0),
	                .("NORMAL",   0, .R8G8B8A8_UNORM,  0, 12, .VERTEX_DATA, 0),
	                .("TANGENT",  0, .R8G8B8A8_UNORM,  0, 16, .VERTEX_DATA, 0),
	                .("TEXCOORD", 0, .R16G16_SINT,     0, 20, .VERTEX_DATA, 0),
	            ), false);

            CreateMaterial(device, "Rock", "Rock.hlsl", //Note: This is just Pixlit1UvNmap with a specular texture. Use shader includes to get rid of duplicate code here
	            D3D11_INPUT_ELEMENT_DESC[]
	            (
	                .("POSITION", 0, .R32G32B32_FLOAT, 0, 0,  .VERTEX_DATA, 0),
	                .("NORMAL",   0, .R8G8B8A8_UNORM,  0, 12, .VERTEX_DATA, 0),
	                .("TANGENT",  0, .R8G8B8A8_UNORM,  0, 16, .VERTEX_DATA, 0),
	                .("TEXCOORD", 0, .R16G16_SINT, 0,  20,    .VERTEX_DATA, 0),
	            ), false);

            CreateMaterial(device, "Pixlit1UvNmapCa", "Pixlit1UvNmapCa.hlsl",
	            D3D11_INPUT_ELEMENT_DESC[]
	            (
	                .("POSITION",    0, .R32G32B32_FLOAT, 0, 0,  .VERTEX_DATA, 0),
	                .("NORMAL",      0, .R8G8B8A8_UNORM,  0, 12, .VERTEX_DATA, 0),
                    .("TANGENT",     0, .R8G8B8A8_UNORM,  0, 16, .VERTEX_DATA, 0),
                    .("BLENDWEIGHT", 0, .R8G8B8A8_UNORM,  0, 20, .VERTEX_DATA, 0),
	                .("BLENDINDEX",  0, .R8G8B8A8_UINT,   0, 24, .VERTEX_DATA, 0),
	                .("TEXCOORD",    0, .R16G16_SINT,     0, 28, .VERTEX_DATA, 0),
	            ), false);

            CreateMaterial(device, "Pixlit2UvNmap", "Pixlit2UvNmap.hlsl",
	            D3D11_INPUT_ELEMENT_DESC[]
	            (
	                .("POSITION", 0, .R32G32B32_FLOAT, 0, 0,  .VERTEX_DATA, 0),
	                .("NORMAL",   0, .R8G8B8A8_UNORM,  0, 12, .VERTEX_DATA, 0),
	                .("TANGENT",  0, .R8G8B8A8_UNORM,  0, 16, .VERTEX_DATA, 0),
                    .("TEXCOORD", 0, .R16G16_SINT,     0, 20, .VERTEX_DATA, 0),
                    .("TEXCOORD", 1, .R16G16_SINT,     0, 24, .VERTEX_DATA, 0),
	            ), false);

            CreateMaterial(device, "Pixlit3UvNmap", "Pixlit3UvNmap.hlsl",
	            D3D11_INPUT_ELEMENT_DESC[]
	            (
	                .("POSITION", 0, .R32G32B32_FLOAT, 0, 0,  .VERTEX_DATA, 0),
	                .("NORMAL", 0,   .R8G8B8A8_UNORM,  0, 12, .VERTEX_DATA, 0),
	                .("TANGENT", 0,  .R8G8B8A8_UNORM,  0, 16, .VERTEX_DATA, 0),
	                .("TEXCOORD", 0, .R16G16_SINT,     0, 20, .VERTEX_DATA, 0),
                    .("TEXCOORD", 1, .R16G16_SINT,     0, 24, .VERTEX_DATA, 0),
	                .("TEXCOORD", 2, .R16G16_SINT,     0, 28, .VERTEX_DATA, 0),
	            ), false);

            CreateMaterial(device, "Pixlit3UvNmapCa", "Pixlit3UvNmapCa.hlsl",
	            D3D11_INPUT_ELEMENT_DESC[]
	            (
	                .("POSITION",    0, .R32G32B32_FLOAT, 0, 0,  .VERTEX_DATA, 0),
	                .("NORMAL",      0, .R8G8B8A8_UNORM,  0, 12, .VERTEX_DATA, 0),
	                .("TANGENT",     0, .R8G8B8A8_UNORM,  0, 16, .VERTEX_DATA, 0),
                    .("BLENDWEIGHT", 0, .R8G8B8A8_UNORM,  0, 20, .VERTEX_DATA, 0),
                    .("BLENDINDEX",  0, .R8G8B8A8_UINT,   0, 24, .VERTEX_DATA, 0),
	                .("TEXCOORD",    0, .R16G16_SINT,     0, 28, .VERTEX_DATA, 0),
	                .("TEXCOORD",    1, .R16G16_SINT,     0, 32, .VERTEX_DATA, 0),
	                .("TEXCOORD",    2, .R16G16_SINT,     0, 36, .VERTEX_DATA, 0),
	            ), false);

            //Primitive formats
            CreateMaterial(device, "Linelist", "Linelist.hlsl",
	            D3D11_INPUT_ELEMENT_DESC[]
	            (
	                .("POSITION", 0, .R32G32B32A32_FLOAT, 0, 0,  .VERTEX_DATA, 0),
	                .("COLOR",    0, .R8G8B8A8_UNORM,  0, 16, .VERTEX_DATA, 0),
	            ), enableGeometryShaders: true);
            CreateMaterial(device, "SolidTriList", "SolidTriList.hlsl",
	            D3D11_INPUT_ELEMENT_DESC[]
	            (
	                .("POSITION",  0, .R32G32B32_FLOAT, 0, 0,  .VERTEX_DATA, 0),
	                .("COLOR",     0, .R8G8B8A8_UNORM,  0, 12, .VERTEX_DATA, 0),
	            ), false);
            CreateMaterial(device, "LitTriList", "LitTriList.hlsl",
	            D3D11_INPUT_ELEMENT_DESC[]
	            (
                    .("POSITION", 0, .R32G32B32_FLOAT, 0, 0,  .VERTEX_DATA, 0),
	                .("NORMAL",   0, .R32G32B32_FLOAT, 0, 12, .VERTEX_DATA, 0),
	                .("COLOR",    0, .R8G8B8A8_UNORM,  0, 24, .VERTEX_DATA, 0),
	            ), false);
        }

        private static void CreateMaterial(ID3D11Device* device, StringView materialName, StringView shaderName, Span<D3D11_INPUT_ELEMENT_DESC> layout, bool enableGeometryShaders = false)
        {
            Material material = new .();
            if (material.Init(device, shaderName, layout, enableGeometryShaders) case .Err)
            {
                delete material;
                Logger.Fatal("Failed to create render material '{}'", materialName);
                return;
            }

            _materials[materialName] = material;
        }

        public static Result<Material> GetMaterial(StringView name)
        {
            if (_materials.ContainsKey(name))
                return _materials[name];
            else
                return .Err;
        }

        public static void ReloadEditedShaders()
        {
            for (var kv in _materials)
                kv.value.TryReloadShaders();
        }
	}
}