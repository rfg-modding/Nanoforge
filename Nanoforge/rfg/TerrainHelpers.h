#pragma once
#include "common/Typedefs.h"
#include <RfgTools++\formats\meshes\MeshDataBlock.h>
#include <RfgTools++\formats\textures\PegFile10.h>
#include <RfgTools++/formats/meshes/TerrainLowLod.h>
#include <RfgTools++/formats/meshes/Terrain.h>
#include <vector>
#include <span>

struct LowLodTerrainVertex
{
    i16 x = 0;
    i16 y = 0;
    i16 z = 0;
    i16 w = 0;
};
static_assert(sizeof(LowLodTerrainVertex) == 8, "LowLodTerrainVertex size incorrect!");

struct TerrainVertex
{
    u32 Position; //Two i16 values. Decompressed into xyz pos in the vertex shader
    u32 Normal; //4 u8 values. Scaled to normalized float3 in the vertex shader
};
static_assert(sizeof(TerrainVertex) == 8, "TerrainVertex size incorrect!");

struct TerrainSubzone
{
    Terrain Data;
    MeshInstanceData InstanceData;
};

//Data for a single zones terrain. Made up of 9 smaller meshes which are stitched together
struct TerrainInstance
{
    string Name;
    TerrainLowLod DataLowLod;
    std::vector<MeshInstanceData> LowLodMeshes = {};
    std::vector<TerrainSubzone> Subzones = {};
    bool Visible = true;
    bool NeedsRenderInit = false;
    Vec3 Position;

    //If true BlendTextureBytes has data
    bool HasBlendTexture = false;
    //Peg file for blend texture
    PegFile10 BlendPeg;
    //PC_8888 pixel data (DXGI_FORMAT_R8G8B8A8_UNORM)
    std::span<u8> BlendTextureBytes;
    //Blend texture dimensions
    u32 BlendTextureWidth = 0;
    u32 BlendTextureHeight = 0;

    bool HasTexture1 = false;
    PegFile10 Texture1;
    std::span<u8> Texture1Bytes;
    u32 Texture1Width = 0;
    u32 Texture1Height = 0;

    //Index of this terrain subpiece on 3x3 grid that makes up the terrain of a single zone
    int TerrainSubpieceIndex = 0;

    //TODO: ***********REMOVE BEFORE COMITTING***************
    bool Success = false;
};