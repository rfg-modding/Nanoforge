using System.Numerics;
using Nanoforge.Editor;

namespace Nanoforge.Rfg;

public class Rock : EditorObject
{
    public Vector3 Position;
    public Matrix4x4 Rotation;
    public ProjectMesh? Mesh;
    public ProjectTexture? DiffuseTexture;
    //public ProjectTexture? NormalTexture;

    public Rock()
    {
        
    }
    
    public Rock(string name, Vector3 position, Matrix4x4 rotation, ProjectMesh mesh, ProjectTexture? diffuseTexture = null)
    {
        Name = name;
        Position = position;
        Rotation = rotation;
        Mesh = mesh;
        DiffuseTexture = diffuseTexture;
    }
}