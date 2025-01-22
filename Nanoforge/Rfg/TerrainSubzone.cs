using System.Numerics;
using Nanoforge.Editor;

namespace Nanoforge.Rfg;

//Terrain subzone. Each zone consists of 9 subzones
public class TerrainSubzone : EditorObject
{
    public ProjectMesh? Mesh;
    //Splat material textures per subzone. The splatmap has 4 channels, so it supports 4 material textures.
    public ProjectTexture[] SplatMaterialTextures = new ProjectTexture[8];
    public Vector3 Position;

    public bool HasStitchMeshes = false;
    public ProjectMesh? StitchMesh;
}