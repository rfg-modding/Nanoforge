using System;
using System.Collections.Generic;
using System.Linq;
using Nanoforge.Render.Resources;

namespace Nanoforge.Render;

//Tracks all textures used by the renderer and their reference count
public static class TextureManager
{
    public const int MaxTextures = 8192;
    
    public class TextureMetadata
    {
        public string Name;
        public readonly Texture2D Texture;
        public int ReferenceCount;
        public bool NeverDestroy { get; private set; }

        public TextureMetadata(string name, Texture2D texture, bool neverDestroy = false)
        {
            Name = name;
            Texture = texture;
            ReferenceCount = 0;
            NeverDestroy = neverDestroy;
        }
    }

    public static List<TextureMetadata> Textures = new();

    public static bool IsTextureLoaded(string textureName)
    {
        return Textures.Any(t => t.Name == textureName);
    }

    public static void NewTexture(string textureName, Texture2D texture, bool neverDestroy = false)
    {
        if (Textures.Count == MaxTextures)
        {
            throw new Exception($"Exceeded maximum texture count of {MaxTextures}.");
        }
        if (Textures.Any(textureMetadata => textureMetadata.Name == textureName))
        {
            return;
        }
        
        var metadata = new TextureMetadata(textureName, texture, neverDestroy)
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
        if (metadata is { ReferenceCount: 0, NeverDestroy: false })
        {
            metadata.Texture.Destroy();
            Textures.Remove(metadata);
        }
    }

    public static void DestroyUnusedTextures()
    {
        foreach (TextureMetadata metadata in Textures.Where(metadata => metadata is { ReferenceCount: 0, NeverDestroy: false }).ToArray())
        {
            Textures.Remove(metadata);
            metadata.Texture.Destroy();
        }
    }
}