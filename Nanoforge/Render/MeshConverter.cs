using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Numerics;
using System.Runtime.CompilerServices;
using Nanoforge.Render.Resources;
using Nanoforge.Render.VertexFormats;
using Nanoforge.Render.VertexFormats.Rfg;
using RFGM.Formats.Meshes.Shared;
using Silk.NET.Vulkan;

namespace Nanoforge.Render;

//Converts RFG mesh data to a single vertex & index format used by the renderer. Using one format simplifies the renderer and shader code.
public class MeshConverter
{
    private unsafe delegate UnifiedVertex VertexConverterFunc(byte* sourceBytes);
    
    public RenderMeshData Convert(MeshInstanceData mesh)
    {
        return Convert(mesh.Vertices, mesh.Indices, mesh.Config.NumVertices, mesh.Config.NumIndices, mesh.Config.IndexSize, mesh.Config.VertexFormat, mesh.Config.VertexStride0,
                       mesh.Config.Submeshes, mesh.Config.RenderBlocks);
    }

    public RenderMeshData Convert(byte[] vertices, byte[] indices, uint numVertices, uint numIndices, byte indexSize, VertexFormat vertexFormat, byte vertexStride, List<SubmeshData> submeshes, List<RenderBlock> renderBlocks)
    {
        //Converts all meshes to use u32 indices. Most RFG meshes only need u16, but some buildings require u32. This uses u32 on all of them to simplify things.
        byte[] newVertices = ConvertRfgVerticesToUnifiedVertexFormat(vertices, vertexFormat, numVertices, vertexStride);
        Debug.Assert(newVertices.Length == numVertices * Unsafe.SizeOf<UnifiedVertex>());

        byte[] newIndices = indexSize switch
        {
            2 => ConvertU16IndicesToU32(indices),
            4 => indices,
            _ => throw new Exception($"Unsupported index size of {indexSize} bytes for input mesh in MeshConverter.Convert()"),
        };
        Debug.Assert(newIndices.Length == numIndices * Unsafe.SizeOf<uint>());

        return new RenderMeshData(submeshes.ToList(), renderBlocks.ToList(), IndexType.Uint32, newVertices, newIndices, numVertices, numIndices);
    }

    //TODO: Generate normals when not available in source mesh.
    //TODO: Generate tangents when not available and replace the ones in the source meshes since they appear to be broken in the vanilla meshes.
    private unsafe byte[] ConvertRfgVerticesToUnifiedVertexFormat(byte[] meshVertices, VertexFormat vertexFormat, uint numVertices, byte vertexStride)
    {
        VertexConverterFunc converter = vertexFormat switch
        {
            VertexFormat.HeightMesh => sourceBytes =>
            {
                HighLodTerrainVertex* sourceVertex = (HighLodTerrainVertex*)sourceBytes;
                
                //Note: inPosition.xy is 2 16 bit integers. Initial range: [-32768, 32767]
                //      inPosition.x contains the final y value, and inPosition.y contains the final x and z values

                //Adjust inPosition value range and extract upper byte
                int xz = sourceVertex->PosY + 32768; //Adjust range to [0, 65535]
                int xzUpper = xz & 0xFF00; //Zero the lower byte. Range stays at [0, 65535]
                int xzUpperScaled = xzUpper >> 8; //Left shift 8 bits to adjust range to [0, 256]

                //Final position
                Vector3 posFinal = Vector3.Zero;
                posFinal.X = xz - xzUpper; //Difference between xz and the upper byte of xz
                posFinal.Y = (float)sourceVertex->PosX / 64.0f; //Divide by 64 to scale y to [-512.0, 512.0]
                posFinal.Z = xzUpperScaled;
                
                UnifiedVertex newVertex = new()
                {
                    Position = posFinal,
                    NormalX = sourceVertex->NormalX, NormalY = sourceVertex->NormalY, NormalZ = sourceVertex->NormalZ, NormalW = sourceVertex->NormalW,
                    TangentX = 0, TangentY = 0, TangentZ = 0, TangentW = 0,
                    UvX = 0, UvY = 0, //These are generated in the shader. Is dependent of the position of each subzone
                };
                return newVertex;
            },
            
            VertexFormat.HeightMeshLowLod => sourceBytes =>
            {
                LowLodTerrainVertex* sourceVertex = (LowLodTerrainVertex*)sourceBytes;

                //Scale terrain mesh range (-32,768 to 32,767) to zone range (-255.5f to 255.5f as coordinates relative to zone center)
                //In other words: Convert vertices from terrain local coords to zone local coords
                //Range of terrain vertex component values
                float terrainMin = -32768.0f;
                float terrainMax = 32767.0f;
                //Range of values relative to a zones center
                float zoneMin = -255.5f;
                float zoneMax = 255.5f;
                //Ranges
                float terrainRange = terrainMax - terrainMin;
                terrainRange *= 0.9980f; //Hack to fill in gaps between zones. May cause object positions to be slightly off
                float zoneRange = zoneMax - zoneMin;

                //Scale position
                Vector4 posFinal = new Vector4(sourceVertex->PosX, sourceVertex->PosY, sourceVertex->PosZ, sourceVertex->PosW);
                posFinal.X = (((posFinal.X - terrainMin) * zoneRange) / terrainRange) + zoneMin;
                posFinal.Y = (((posFinal.Y - terrainMin) * zoneRange) / terrainRange) + zoneMin;
                posFinal.Z = (((posFinal.Z - terrainMin) * zoneRange) / terrainRange) + zoneMin;
                posFinal.W = 1.0f;
                posFinal.Y *= 2.0f;
                
                //Calc texture UV
                Vector2 uv = new Vector2();
                uv.X = ((posFinal.X + 255.5f) / 511.0f);
                uv.Y = ((posFinal.Z + 255.5f) / 511.0f);
                uv.X *= 0.9980f; //Part of hack to fix terrain gaps. Must scale UVs since we're scaling the terrain size
                uv.Y *= 0.9980f;
                
                UnifiedVertex newVertex = new()
                {
                    Position = new Vector3(posFinal.X, posFinal.Y, posFinal.Z),
                    NormalX = 0, NormalY = 0, NormalZ = 0, NormalW = 0,
                    TangentX = 0, TangentY = 0, TangentZ = 0, TangentW = 0,
                    UvX = (short)(uv.X / short.MaxValue), UvY = (short)(uv.Y / short.MaxValue),
                };
                
                return newVertex;
            },
            VertexFormat.Pixlit => sourceBytes =>
            {
                PixlitVertex* sourceVertex = (PixlitVertex*)sourceBytes;
                
                UnifiedVertex newVertex = new()
                {
                    Position = sourceVertex->Position,
                    NormalX = sourceVertex->NormalX, NormalY = sourceVertex->NormalY, NormalZ = sourceVertex->NormalZ, NormalW = sourceVertex->NormalW,
                    TangentX = 0, TangentY = 0, TangentZ = 0, TangentW = 0,
                    UvX = 0, UvY = 0,
                };
                
                return newVertex;
            },
            VertexFormat.Pixlit1Uv => sourceBytes =>
            {
                Pixlit1UvVertex* sourceVertex = (Pixlit1UvVertex*)sourceBytes;
                
                UnifiedVertex newVertex = new()
                {
                    Position = sourceVertex->Position,
                    NormalX = sourceVertex->NormalX, NormalY = sourceVertex->NormalY, NormalZ = sourceVertex->NormalZ, NormalW = sourceVertex->NormalW,
                    TangentX = 0, TangentY = 0, TangentZ = 0, TangentW = 0,
                    UvX = sourceVertex->UvX, UvY = sourceVertex->UvY,
                };
                
                return newVertex;
            },
            VertexFormat.Pixlit1UvNmap => sourceBytes =>
            {
                Pixlit1UvNmapVertex* sourceVertex = (Pixlit1UvNmapVertex*)sourceBytes;
                
                UnifiedVertex newVertex = new()
                {
                    Position = sourceVertex->Position,
                    NormalX = sourceVertex->NormalX, NormalY = sourceVertex->NormalY, NormalZ = sourceVertex->NormalZ, NormalW = sourceVertex->NormalW,
                    TangentX = sourceVertex->TangentX, TangentY = sourceVertex->TangentY, TangentZ = sourceVertex->TangentZ, TangentW = sourceVertex->TangentW,
                    UvX = sourceVertex->UvX, UvY = sourceVertex->UvY,
                };
                
                return newVertex;
            },
            VertexFormat.Pixlit2UvNmap => sourceBytes =>
            {
                Pixlit2UvNmapVertex* sourceVertex = (Pixlit2UvNmapVertex*)sourceBytes;
                
                UnifiedVertex newVertex = new()
                {
                    Position = sourceVertex->Position,
                    NormalX = sourceVertex->NormalX, NormalY = sourceVertex->NormalY, NormalZ = sourceVertex->NormalZ, NormalW = sourceVertex->NormalW,
                    TangentX = sourceVertex->TangentX, TangentY = sourceVertex->TangentY, TangentZ = sourceVertex->TangentZ, TangentW = sourceVertex->TangentW,
                    UvX = sourceVertex->Uv0X, UvY = sourceVertex->Uv0Y,
                };
                
                return newVertex;
            },
            VertexFormat.Pixlit3UvNmap => sourceBytes =>
            {
                Pixlit3UvNmapVertex* sourceVertex = (Pixlit3UvNmapVertex*)sourceBytes;
                
                UnifiedVertex newVertex = new()
                {
                    Position = sourceVertex->Position,
                    NormalX = sourceVertex->NormalX, NormalY = sourceVertex->NormalY, NormalZ = sourceVertex->NormalZ, NormalW = sourceVertex->NormalW,
                    TangentX = sourceVertex->TangentX, TangentY = sourceVertex->TangentY, TangentZ = sourceVertex->TangentZ, TangentW = sourceVertex->TangentW,
                    UvX = sourceVertex->Uv0X, UvY = sourceVertex->Uv0Y,
                };
                
                return newVertex;
            },
            VertexFormat.Pixlit4UvNmap => sourceBytes =>
            {
                Pixlit4UvNmapVertex* sourceVertex = (Pixlit4UvNmapVertex*)sourceBytes;
                
                UnifiedVertex newVertex = new()
                {
                    Position = sourceVertex->Position,
                    NormalX = sourceVertex->NormalX, NormalY = sourceVertex->NormalY, NormalZ = sourceVertex->NormalZ, NormalW = sourceVertex->NormalW,
                    TangentX = sourceVertex->TangentX, TangentY = sourceVertex->TangentY, TangentZ = sourceVertex->TangentZ, TangentW = sourceVertex->TangentW,
                    UvX = sourceVertex->Uv0X, UvY = sourceVertex->Uv0Y,
                };
                
                return newVertex;
            },
            //TODO: Get list of all vertex formats used by game and add support for them all. It's possible that the game doesn't use every option in the VertexFormat enum.
            _ => throw new Exception($"Unsupported vertex format: {vertexFormat} in MeshConverter."),
        };

        byte[] newVertices = new byte[numVertices * Unsafe.SizeOf<UnifiedVertex>()];
        fixed (byte* newVerticesPtr = newVertices)
        fixed (byte* sourceVerticesPtr = meshVertices)
        {
            UnifiedVertex* curNewVertex = (UnifiedVertex*)newVerticesPtr;
            byte* curSourceVertex = sourceVerticesPtr;
            for (int i = 0; i < numVertices; i++)
            {
                *curNewVertex = converter(curSourceVertex);
                curSourceVertex += vertexStride;
                curNewVertex++;
            }
        }
        
        return newVertices;
    }

    private unsafe byte[] ConvertU16IndicesToU32(byte[] u16Indices)
    {
        int numIndices = u16Indices.Length / sizeof(ushort);
        byte[] newIndicesBytes = new byte[numIndices * sizeof(uint)];

        fixed (byte* newIndicesPtr = newIndicesBytes)
        fixed (byte* u16IndexPtr = u16Indices)
        {
            ushort* curU16Index = (ushort*)u16IndexPtr;
            uint* curU32Index = (uint*)newIndicesPtr;
            for (int i = 0; i < numIndices; i++)
            {
                *curU32Index = *curU16Index;
                curU16Index++;
                curU32Index++;
            }
        }
        
        return newIndicesBytes;
    }
}