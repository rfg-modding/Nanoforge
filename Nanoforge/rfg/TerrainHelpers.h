#pragma once
#include "common/Typedefs.h"
#include "common/Handle.h"
#include <RfgTools++/formats/meshes/TerrainLowLod.h>
#include <RfgTools++/formats/meshes/Terrain.h>
#include <vector>
#include <mutex>

class RenderObject;

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

struct StitchMesh
{
    Handle<RenderObject> Mesh;
    u32 SubzoneIndex;
};

struct RoadMesh
{
    Handle<RenderObject> Mesh;
    u32 SubzoneIndex;
};

//Data for a single zones terrain. Made up of 9 smaller meshes which are stitched together
struct TerrainInstance
{
    string Name;
    TerrainLowLod DataLowLod;
    std::vector<Terrain> Subzones = {};
    bool Visible = true;
    Vec3 Position;

    //Low lod
    std::vector<Handle<RenderObject>> LowLodMeshes;

    //High lod
    std::vector<Handle<RenderObject>> HighLodMeshes;
    std::vector<StitchMesh> StitchMeshes;
    std::vector<RoadMesh> RoadMeshes;
};