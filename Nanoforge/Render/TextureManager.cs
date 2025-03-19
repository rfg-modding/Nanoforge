using System;
using System.Collections.Generic;
using System.Linq;
using Nanoforge.Render.Resources;

namespace Nanoforge.Render;

//Tracks all textures used by the renderer and their reference count
public static class TextureManager
{
    //Note: If you change this, also update the texture array size in Constants.glsl
    public const int MaxTextures = 8192;
    
    public static bool DescriptorSetsNeedUpdate = false;
    
    public class TextureSlot
    {
        public string? TextureName;
        public Texture2D? Texture;
        public int ReferenceCount = 0;
        public bool InUse { get; set; } = false;
        public bool NeverDestroy { get; set; } = false;
        public readonly int Index;

        public TextureSlot(int index)
        {
            Index = index;
        }
    }

    public static TextureSlot[] TextureSlots = new TextureSlot[MaxTextures];

    static TextureManager()
    {
        for (var index = 0; index < TextureSlots.Length; index++)
        {
            TextureSlot slot = new(index)
            {
                InUse = false,
                ReferenceCount = 0,
                
            };
            TextureSlots[index] = slot;
        }
    }
    
    public static bool IsTextureLoaded(string textureName)
    {
        return TextureSlots.Any(t => t.TextureName == textureName);
    }

    public static void NewTexture(string textureName, Texture2D texture, bool neverDestroy = false)
    {
        TextureSlot? slot = GetNextOpenSlot();
        if (slot == null)
        {
            throw new Exception($"Exceeded maximum texture count of {MaxTextures}.");
        }
        if (TextureSlots.Any(textureMetadata => textureMetadata.TextureName == textureName))
        {
            return;
        }

        slot.TextureName = textureName;
        slot.Texture = texture;
        slot.ReferenceCount = 1;
        slot.InUse = true;
        slot.NeverDestroy = neverDestroy;
        texture.Index = slot.Index;
        DescriptorSetsNeedUpdate = true;
    }

    private static TextureSlot? GetNextOpenSlot()
    {
        return TextureSlots.FirstOrDefault(slot => !slot.InUse);
    }

    public static Texture2D? GetTexture(string textureName)
    {
        TextureSlot? metadata = TextureSlots.FirstOrDefault(slot => slot.TextureName == textureName && slot.InUse);
        if (metadata != null)
        {
            metadata.ReferenceCount++;
        }
        
        return metadata?.Texture;
    }

    public static void RemoveReference(Texture2D texture)
    {
        TextureSlot? slot = TextureSlots.FirstOrDefault(slot => slot.Texture == texture && slot.InUse);
        if (slot == null) 
            return;
        
        slot.ReferenceCount = Math.Clamp(--slot.ReferenceCount, 0, int.MaxValue);
        if (slot is { ReferenceCount: 0, NeverDestroy: false })
        {
            slot.Texture!.Destroy();
            slot.Texture = null;
            slot.InUse = false;
            slot.TextureName = null;
            DescriptorSetsNeedUpdate = true;
        }
    }

    public static void DestroyUnusedTextures()
    {
        foreach (TextureSlot slot in TextureSlots.Where(metadata => metadata is { ReferenceCount: 0, NeverDestroy: false, InUse: true, Texture: not null }).ToArray())
        {
            slot.Texture!.Destroy();
            slot.Texture = null;
            slot.InUse = false;
            slot.TextureName = null;
            slot.ReferenceCount = 0;
        }
        DescriptorSetsNeedUpdate = true;
    }
}