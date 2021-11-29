#pragma once
#include "common/Typedefs.h"
#include <RfgTools++/formats/textures/PegFile10.h>
#include "render/resources/Texture2D.h"
#include <optional>

class Task;
class PackfileVFS;
class Packfile3;

//Used by some search functions in TextureIndex
struct TextureSearchResult
{
    PegFile10 Peg;
    u32 TextureIndex;
};

//Performs quick texture searches using a pre-generated texture index
class TextureIndex
{
public:
    void Init(PackfileVFS* packfileVFS) { packfileVFS_ = packfileVFS; }
    //Attempt to load a texture index from a file in the Nanoforge folder
    void Load();

    //Locate a peg
    std::optional<string> FindPeg(const std::string_view pegName) const;
    //Locate a texture. Returns the path of the texture and the peg that contains it. E.g. `misc.vpp_pc/always_loaded.cpeg_pc/menu_background.tga'
    //Optionally can look for a high res variant of the texture by removing _low from it's name.
    std::optional<string> FindTexture(const std::string_view textureName, bool tryHighResVariant = false) const;
    //Returns the path of the peg that contains the texture
    std::optional<string> GetTexturePegPath(const std::string_view textureName, bool tryHighResVariant = false) const;
    //Load a peg. Caller must cleanup PegFile10 if successful
    std::optional<PegFile10> GetPeg(const std::string_view pegName) const;
    //Load a peg using its full path. Caller must cleanup PegFile10 if successful.
    std::optional<PegFile10> GetPegFromPath(const std::string_view pegPath) const;
    //Load the peg that contains a texture. Caller must cleanup PegFile10 if successful
    std::optional<TextureSearchResult> GetTexturePeg(const std::string_view textureName, bool tryHighResVariant = false) const;
    //Load the pixel data of a texture. Caller must free span if successful.
    std::optional<std::span<u8>> GetTexturePixels(const std::string_view textureName, bool tryHighResVariant = false) const;
    //Load a texture and create a render texture (Texture2D) from it
    std::optional<Texture2D> GetRenderTexture(const std::string_view textureName, ComPtr<ID3D11Device> d3d11Device, bool tryHighResVariant = false) const;

    //Start texture database generation background task
    void StartTextureIndexGeneration();

    Handle<Task> TextureIndexGenTask = nullptr;
    string TextureIndexGenTaskStatus;
    f32 TextureIndexGenTaskProgressFraction = 0.0f;

private:
    //Only used during texture search database generation
    using PackfileTextureData = std::vector<std::tuple<string, u64, std::vector<u64>>>;

    //Tasks for generating texture database
    void TextureIndexGenerationTask(Handle<Task> task);
    void TextureIndexGenerationSubtask(Handle<Task> task, Handle<Packfile3> packfile, PackfileTextureData* textureData);

    PackfileVFS* packfileVFS_ = nullptr;

    struct PegData
    {
        u64 NameHash; //Hash of lowercase name string
        u32 PathIndex; //Peg index in pegData_
    };
    struct TgaData
    {
        u64 NameHash; //Hash of lowercase name string
        u32 PegIndex; //Parent peg index in pegData_
    };
    std::span<TgaData> tgaData_;
    std::span<PegData> pegData_;
    std::span<char> pegPaths_;
    std::vector<std::string_view> pegPathsView_;
};