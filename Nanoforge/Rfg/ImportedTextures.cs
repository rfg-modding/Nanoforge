using System.Collections.Generic;
using Nanoforge.Editor;

namespace Nanoforge.Rfg;

//List of peg subtextures imported and stored in project buffers. Used to prevent repeat imports. Only one instance of this should exist
//TODO: Use the full path instead of subtexture name as the key to properly handle textures with the same name button different contents/size
public class ImportedTextures : EditorObject
{
    private object _newTextureLock = new();
    public List<ProjectTexture> Textures = new();

    public ProjectTexture? GetTexture(string name)
    {
        lock (_newTextureLock)
        {
            foreach (ProjectTexture texture in Textures)
            {
                if (texture.Name == name)
                {
                    return texture;
                }
            }
            return null;
        }
    }

    public void AddTexture(ProjectTexture texture)
    {
        lock (_newTextureLock)
        {
            Textures.Add(texture);
        }
    }
}