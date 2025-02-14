using System.Numerics;
using Nanoforge.Editor;

namespace Nanoforge.Rfg;

//Terrain subzone. Each zone consists of 9 subzones
public class TerrainSubzone : EditorObject
{
    public ProjectMesh? Mesh;
    public Vector3 Position;

    public bool HasStitchMeshes = false;
    public ProjectMesh? StitchMesh;
}