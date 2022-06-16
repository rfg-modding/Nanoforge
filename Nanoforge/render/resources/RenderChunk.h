#pragma once
#include "common/Typedefs.h"
#include "RenderObject.h"
#include <RfgTools++/types/Vec3.h>
#include <RfgTools++/types/Mat3.h>
#include <RfgTools++/formats/meshes/ChunkMesh.h>
#include <RfgTools++/formats/meshes/ChunkTypes.h>
#include <vector>

struct ChunkPiece
{
    Vec3 Position;
    Mat3 Orient;
    u16 FirstSubpiece;
    u8 NumSubpieces;
};

struct RenderChunkData
{
    std::vector<ChunkPiece> Pieces = {};
    std::vector<u16> SubpieceSubmeshes = {};
};

//Renders an RFG chunk mesh (cchh_pc file)
class RenderChunk : public RenderObject
{
public:
    RenderChunk(const Mesh& mesh, const RenderChunkData& chunkData, const Vec3& position, const Mat3& rotation = {});
    void Draw(ComPtr<ID3D11DeviceContext> d3d11Context, Buffer& perObjectBuffer, Camera& cam) override;

private:
    RenderChunkData _chunkData;
};