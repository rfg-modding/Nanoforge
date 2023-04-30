using Common;
using System;
using Common.Math;
using Nanoforge.Rfg;
using Direct3D;

namespace Nanoforge.Render.Resources
{
	public class RenderChunk : RenderObject
	{
        private ChunkVariant _chunkData = null;

        public this(Mesh mesh, Vec3 position, Mat3 rotation, ChunkVariant variant) : base(mesh, position, rotation)
        {
            _chunkData = variant;
        }

        public override void Draw(ID3D11DeviceContext* context, Buffer perObjectBuffer, Camera3D cam)
        {
            if (!Visible)
                return;

            //TODO: Get material data for chunks and apply proper textures. Currently it uses the same texture for the entire mesh. 
            //Bind textures
            if (UseTextures)
            {
                for (u32 i in 0 ... 9)
                {
                    if (Textures[i] != null)
                    {
                        Textures[i].Bind(context, i);
                    }
                }
            }

            //Translation and rotation for the overall object
            Mat4 objRotation = .();
            objRotation.Vectors[0] = .(Rotation.Vectors[0].x, Rotation.Vectors[0].y, Rotation.Vectors[0].z, 0.0f);
            objRotation.Vectors[1] = .(Rotation.Vectors[1].x, Rotation.Vectors[1].y, Rotation.Vectors[1].z, 0.0f);
            objRotation.Vectors[2] = .(Rotation.Vectors[2].x, Rotation.Vectors[2].y, Rotation.Vectors[2].z, 0.0f);
            objRotation.Vectors[3] = .(0.0f, 0.0f, 0.0f, 1.0f);
            Mat4 objTranslation = Mat4.Translation(Position);

            //Render each chunk piece
            for (ref ChunkVariant.PieceInstance piece in ref _chunkData.Pieces)
            {
                PerObjectConstants constants = .();

                //Rotation and translation for this chunk
                Mat4 chunkRotation = .();
                chunkRotation.Vectors[0] = .(piece.Orient.Vectors[0].x, piece.Orient.Vectors[0].y, piece.Orient.Vectors[0].z, 0.0f);
                chunkRotation.Vectors[1] = .(piece.Orient.Vectors[1].x, piece.Orient.Vectors[1].y, piece.Orient.Vectors[1].z, 0.0f);
                chunkRotation.Vectors[2] = .(piece.Orient.Vectors[2].x, piece.Orient.Vectors[2].y, piece.Orient.Vectors[2].z, 0.0f);
                chunkRotation.Vectors[3] = .(0.0f, 0.0f, 0.0f, 1.0f);
                Mat4 chunkTranslation = Mat4.Translation(piece.Position);

                Mat4 totalRotation = chunkRotation * objRotation;
                Mat4 model = chunkRotation * chunkTranslation * objRotation * objTranslation;

                //Calculate final MVP matrix + set other constants
                constants.MVP = Mat4.Transpose(model * cam.View * cam.Projection);
                constants.Rotation = totalRotation;
                constants.WorldPosition = .(piece.Position.x + Position.x, piece.Position.y + Position.y, piece.Position.z + Position.z, 1.0f);

                //Set constants
                perObjectBuffer.SetData(context, &constants);

                //TODO: Come up with a better way of handling chunk mesh rendering. This is just a hack that was carried over from the C++ version
                //Fill list of submeshes to draw for this chunk piece
                int numSubmeshOverrides = 0;
                for (int subpieceIndex = piece.FirstSubmesh; subpieceIndex < piece.FirstSubmesh + piece.NumSubmeshes; subpieceIndex++)
                {
                    u16 submeshIndex = _chunkData.PieceData[subpieceIndex].RenderSubmesh;
                    Mesh.SubmeshOverrideIndices[numSubmeshOverrides++] = submeshIndex;
                }

                ObjectMesh.Draw(context, numSubmeshOverrides);
            }
        }
	}
}