using System;
using System.Collections.Generic;
using System.Linq;
using Nanoforge.Render.Resources;

namespace Nanoforge.Render;

//Tracks all textures used by the renderer and their reference count
public static class TextureManager
{
    public class TextureMetadata
    {
        public string Name;
        public readonly Texture2D Texture;
        public int ReferenceCount;

        public TextureMetadata(string name, Texture2D texture)
        {
            Name = name;
            Texture = texture;
            ReferenceCount = 0;
        }
    }

    public static List<TextureMetadata> Textures = new();

    public static bool IsTextureLoaded(string textureName)
    {
        return Textures.Any(t => t.Name == textureName);
    }

    public static void NewTexture(string textureName, Texture2D texture)
    {
        var metadata = new TextureMetadata(textureName, texture)
        {
            ReferenceCount = 1,
        };
        Textures.Add(metadata);
    }

    public static Texture2D? GetTexture(string textureName)
    {
        TextureMetadata? metadata = Textures.FirstOrDefault(t => t.Name == textureName);
        if (metadata != null)
        {
            metadata.ReferenceCount++;
        }
        
        return metadata?.Texture;
    }

    public static void RemoveReference(Texture2D texture)
    {
        TextureMetadata? metadata = Textures.FirstOrDefault(t => t.Texture == texture);
        if (metadata == null) 
            return;
        
        metadata.ReferenceCount = Math.Clamp(--metadata.ReferenceCount, 0, int.MaxValue);
        if (metadata.ReferenceCount == 0)
        {
            metadata.Texture.Destroy();
            Textures.Remove(metadata);
        }
    }

    public static void DestroyUnusedTextures()
    {
        foreach (TextureMetadata metadata in Textures.Where(metadata => metadata.ReferenceCount == 0).ToArray())
        {
            Textures.Remove(metadata);
            metadata.Texture.Destroy();
        }
    }
}