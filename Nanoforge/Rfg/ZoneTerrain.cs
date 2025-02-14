using System.Collections.Generic;
using System.Numerics;
using Nanoforge.Editor;

namespace Nanoforge.Rfg;

public class ZoneTerrain: EditorObject
{
    public Vector3 Position;
    
    public ProjectMesh[] LowLodTerrainMeshes = new ProjectMesh[9];
    public ProjectTexture? CombTexture = null;
    public ProjectTexture? OvlTexture = null;
    public ProjectTexture? Splatmap = null;

    public TerrainSubzone[] Subzones = new TerrainSubzone[9];
    public List<string> MaterialNames = [];
}