#pragma once
#include "common/Typedefs.h"
#include <RfgTools++\formats\meshes\MeshDataBlock.h>
#include <vector>
#include <span>

//Stride = 8 bytes
struct LowLodTerrainVertex
{
    i16 x = 0;
    i16 y = 0;
    i16 z = 0;
    i16 w = 0;
};
static_assert(sizeof(LowLodTerrainVertex) == 8, "LowLodTerrainVertex size incorrect!");

//Data for a single zones terrain. Made up of 9 smaller meshes which are stitched together
struct TerrainInstance
{
    string Name;
    std::vector<MeshDataBlock> Meshes = {}; //Low lod terrain files have 9 meshes (not technically submeshes)
    std::vector<std::span<u16>> Indices = {};
    std::vector<std::span<LowLodTerrainVertex>> Vertices = {};
    bool Visible = true;
    Vec3 Position;
};