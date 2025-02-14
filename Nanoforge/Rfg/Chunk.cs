using System.Collections.Generic;
using System.Numerics;
using Nanoforge.Editor;
using RFGM.Formats.Materials;
using RFGM.Formats.Meshes.Chunks;

namespace Nanoforge.Rfg;

public class Chunk : EditorObject
{
    public ProjectMesh? Mesh;
    public ProjectTexture? DiffuseTexture;
    public ProjectTexture? NormalTexture;
    public ProjectTexture? SpecularTexture;
    
    //TODO: Change this to use nanoforge types that only contain the required data. There's a lot of extra data we can discard.
    public List<Destroyable> Destroyables = [];
    public List<RfgMaterial> Materials = [];
    public List<List<ProjectTexture?>> Textures = [];
    public List<ObjectIdentifier> Identifiers = [];

    //public List<ChunkVariant> Variants = [];
    //
    ////Note: Pulls data from RFGM.Formats Subpiece and SubpieceData. Only has the data NF needs at the moment
    //public struct PieceData
    //{
    //    public ushort RenderSubmesh;
    //}
    //
    ////Note: Pulls data from RFGM.Formats Dlod
    //public struct PieceInstance
    //{
    //    public Vector3 Position;
    //    public Matrix4x4 Orient;
    //    public ushort FirstSubmesh;
    //    public byte NumSubmeshes;
    //}
}