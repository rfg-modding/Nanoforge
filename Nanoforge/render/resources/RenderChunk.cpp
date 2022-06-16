#include "RenderChunk.h"
#include "util/Profiler.h"

RenderChunk::RenderChunk(const Mesh& mesh, const RenderChunkData& chunkData, const Vec3& position, const Mat3& rotation)
    : RenderObject(mesh, position, rotation), _chunkData(chunkData)
{

}

void RenderChunk::Draw(ComPtr<ID3D11DeviceContext> d3d11Context, Buffer& perObjectBuffer, Camera& cam)
{
    //TODO: Properly implement for chunks

    if (!Visible)
        return;

    PROFILER_FUNCTION();

    //Bind textures
    if (UseTextures)
    {
        for (u32 i = 0; i < Textures.size(); i++)
        {
            auto texture = Textures[i];
            if (texture.has_value())
                texture.value().Bind(d3d11Context, i);
        }
    }

    for (ChunkPiece& piece : _chunkData.Pieces)
    {
        //Shader constants for this object
        PerObjectConstants constants;

        //Calculate matrices
        DirectX::XMMATRIX objRotation = DirectX::XMMatrixSet
        (
            Rotation.rvec.x, Rotation.rvec.y, Rotation.rvec.z, 0.0f,
            Rotation.uvec.x, Rotation.uvec.y, Rotation.uvec.z, 0.0f,
            Rotation.fvec.x, Rotation.fvec.y, Rotation.fvec.z, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        );
        DirectX::XMMATRIX objTranslation = DirectX::XMMatrixTranslation(Position.x, Position.y, Position.z);
        DirectX::XMMATRIX scale = DirectX::XMMatrixScaling(Scale.x, Scale.y, Scale.z);

        DirectX::XMMATRIX chunkRotation = DirectX::XMMatrixSet
        (
            piece.Orient.rvec.x, piece.Orient.rvec.y, piece.Orient.rvec.z, 0.0f,
            piece.Orient.uvec.x, piece.Orient.uvec.y, piece.Orient.uvec.z, 0.0f,
            piece.Orient.fvec.x, piece.Orient.fvec.y, piece.Orient.fvec.z, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        );
        DirectX::XMMATRIX chunkTranslation = DirectX::XMMatrixTranslation(piece.Position.x, piece.Position.y, piece.Position.z);

        DirectX::XMMATRIX totalRotation = chunkRotation * objRotation;
        DirectX::XMMATRIX totalTranslation = chunkTranslation * objTranslation;

        DirectX::XMMATRIX modelMatrix = DirectX::XMMatrixIdentity();
        modelMatrix = chunkRotation * chunkTranslation * objRotation * objTranslation;


        //Then calculate MVP matrix with Model * View * Projection
        constants.MVP = modelMatrix;//totalRotation * totalTranslation * scale; //First calculate model matrix
        constants.MVP = DirectX::XMMatrixTranspose(constants.MVP * cam.camView * cam.camProjection);
        constants.Rotation = totalRotation;
        constants.WorldPosition.m128_f32[0] = piece.Position.x + Position.x;
        constants.WorldPosition.m128_f32[1] = piece.Position.y + Position.y;
        constants.WorldPosition.m128_f32[2] = piece.Position.z + Position.z;
        constants.WorldPosition.m128_f32[3] = 1.0f;

        //Set constants
        perObjectBuffer.SetData(d3d11Context, &constants);

        //TODO: Get material data for chunks and apply proper textures. Currently it uses the same texture for the entire mesh. 
        //Fill list of submeshes to draw for this chunk piece
        size_t numSubmeshOverrides = 0;
        for (size_t subpieceIndex = piece.FirstSubpiece; subpieceIndex < piece.FirstSubpiece + piece.NumSubpieces; subpieceIndex++)
        {
            u32 submeshIndex = _chunkData.SubpieceSubmeshes[subpieceIndex];
            SubmeshOverrideIndices[numSubmeshOverrides] = submeshIndex;
            numSubmeshOverrides++;
        }

        ObjectMesh.Draw(d3d11Context, numSubmeshOverrides);
    }
}