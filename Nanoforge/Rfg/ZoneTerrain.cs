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
    //Splat material textures per subzone. The splatmap has 4 channels, so it supports 4 material textures.
    public ProjectTexture?[] SplatmapTextures = new ProjectTexture[8];

    public TerrainSubzone[] Subzones = new TerrainSubzone[9];
    public List<string> MaterialNames = [];
}