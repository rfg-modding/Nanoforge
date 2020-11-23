#pragma once
#include "common/Typedefs.h"
#include <RfgTools++\formats\meshes\MeshDataBlock.h>
#include <RfgTools++\formats\textures\PegFile10.h>
#include <vector>
#include <span>

//Stride = 20 bytes
struct LowLodTerrainVertex
{
    //8 bytes
    i16 x = 0;
    i16 y = 0;
    i16 z = 0;
    i16 w = 0;

    //12 bytes
    Vec3 normal;
};
static_assert(sizeof(LowLodTerrainVertex) == 20, "LowLodTerrainVertex size incorrect!");

//Data for a single zones terrain. Made up of 9 smaller meshes which are stitched together
struct TerrainInstance
{
    string Name;
    std::vector<MeshDataBlock> Meshes = {}; //Low lod terrain files have 9 meshes (not technically submeshes)
    std::vector<std::span<u16>> Indices = {};
    std::vector<std::span<LowLodTerrainVertex>> Vertices = {};
    bool Visible = true;
    bool RenderDataInitialized = false;
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

    //Index of this terrain subpiece on 3x3 grid that makes up the terrain of a single zone
    int TerrainSubpieceIndex = 0;
};