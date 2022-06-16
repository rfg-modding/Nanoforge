#include "Importers.h"
#include "rfg/PackfileVFS.h"
#include "rfg/TextureIndex.h"
#include "util/Profiler.h"
#include "common/filesystem/Path.h"
#include "common/String/String.h"
#include <RfgTools++/formats/meshes/ChunkMesh.h>

//Used to stop the import process early by the calling code setting stopSignal to true. Does nothing if stopSignal is nullptr
#define WorkerEarlyStopCheckReturnNullObj() if(stopSignal && *stopSignal) \
{ \
    return NullObjectHandle; \
} \

ObjectHandle ImportChunkTexture(std::string_view textureName, TextureIndex* textureIndex, ObjectHandle textureCache = NullObjectHandle);

ObjectHandle Importers::ImportChunk(std::string_view chunkFilename, PackfileVFS* packfileVFS, TextureIndex* textureIndex, bool* stopSignal, ObjectHandle textureCache)
{
    PROFILER_FUNCTION();
    Registry& registry = Registry::Get();
    ObjectHandle chunk = registry.CreateObject(chunkFilename, "ChunkMesh");

    //Find cchk_pc file
    std::vector<FileHandle> search = packfileVFS->GetFiles(string(chunkFilename), true, true);
    if (search.size() == 0)
    {
        LOG_ERROR("Failed to locate {}", chunkFilename);
        return NullObjectHandle;
    }

    //Load cchk_pc|gchk_pc file pair
    std::optional<FilePair> filePair = search[0].GetPair();
    if (!filePair)
    {
        LOG_ERROR("Failed to load file pair for {}", chunkFilename);
        return NullObjectHandle;
    }

    //Parse source file
    ChunkMesh chunkMesh;
    std::vector<u8>& cpuFileBytes = filePair.value().CpuFile;
    std::vector<u8>& gpuFileBytes = filePair.value().GpuFile;
    try
    {
        chunkMesh.Read(cpuFileBytes, chunkFilename);
    }
    catch (std::exception& ex)
    {
        LOG_ERROR("Exception thrown while parsing chunk file {}. Ex: {}", chunkFilename, ex.what());
        return NullObjectHandle;
    }

    //Import relevant file data
    chunk.Set<u32>("Signature", chunkMesh.Header.Signature);
    chunk.Set<u32>("Version", chunkMesh.Header.Version);
    chunk.Set<u32>("SourceVersion", chunkMesh.Header.SourceVersion);
    chunk.SetStringList("TextureNames", chunkMesh.Textures);
    
    //Import destroyables
    for (Destroyable& destroyable : chunkMesh.Destroyables)
    {
        ObjectHandle destroyableObj = registry.CreateObject("", "Destroyable");
        destroyableObj.Set<f32>("Mass", destroyable.Mass);

        //Import destroyable subpieces
        for (size_t i = 0; i < destroyable.Subpieces.size(); i++)
        {
            Subpiece& subpiece = destroyable.Subpieces[i];
            SubpieceData& subpieceData = destroyable.SubpieceData[i];
            ObjectHandle subpieceObj = registry.CreateObject("", "Subpiece");

            subpieceObj.Set<Vec3>("Bmin", subpiece.Bmin);
            subpieceObj.Set<Vec3>("Bmax", subpiece.Bmax);
            subpieceObj.Set<Vec3>("Position", subpiece.Position);
            subpieceObj.Set<Vec3>("CenterOfMass", subpiece.CenterOfMass);
            subpieceObj.Set<f32>("Mass", subpiece.Mass);
            subpieceObj.Set<u32>("DlodKey", subpiece.DlodKey);
            subpieceObj.Set<u32>("LinksOffset", subpiece.LinksOffset);
            subpieceObj.Set<u8>("PhysicalMaterialIndex", subpiece.PhysicalMaterialIndex);
            subpieceObj.Set<u8>("ShapeType", subpiece.ShapeType);
            subpieceObj.Set<u8>("NumLinks", subpiece.NumLinks);
            subpieceObj.Set<u8>("Flags", subpiece.Flags);
            subpieceObj.Set<u32>("ShapeOffset", subpieceData.ShapeOffset);
            subpieceObj.Set<u16>("CollisionModel", subpieceData.CollisionModel);
            subpieceObj.Set<u16>("RenderSubpiece", subpieceData.RenderSubpiece);
            subpieceObj.Set<u32>("Unknown0", subpieceData.Unknown0);
            destroyableObj.AppendObjectList("Subpieces", subpieceObj);
        }

        //Import links
        for (Link& link : destroyable.Links)
        {
            ObjectHandle linkObj = registry.CreateObject("", "Link");
            linkObj.Set<i32>("YieldMax", link.YieldMax);
            linkObj.Set<f32>("Area", link.Area);
            linkObj.Set<i16>("Obj0", link.Obj[0]);
            linkObj.Set<i16>("Obj1", link.Obj[1]);
            linkObj.Set<u8>("Flags", link.Flags);
            destroyableObj.AppendObjectList("Links", linkObj);
        }

        //Import dlods
        for (Dlod& dlod : destroyable.Dlods)
        {
            ObjectHandle dlodObj = registry.CreateObject("", "Dlod");
            dlodObj.Set<u32>("NameHash", dlod.NameHash);
            dlodObj.Set<Vec3>("Position", dlod.Pos);
            dlodObj.Set<Mat3>("Orient", dlod.Orient);
            dlodObj.Set<u16>("RenderSubpiece", dlod.RenderSubpiece);
            dlodObj.Set<u16>("FirstPiece", dlod.FirstPiece);
            dlodObj.Set<u8>("MaxPieces", dlod.MaxPieces);
            destroyableObj.AppendObjectList("Dlods", dlodObj);
        }

        //TODO: Import RbbNodes once added to ChunkMesh::Read()
        //TODO: Import DestroyableInstanceData once added to ChunkMesh::Read()

        //Import additional destroyable data
        destroyableObj.Set<u32>("UID", destroyable.UID);
        destroyableObj.Set<string>("Name", destroyable.Name);
        destroyableObj.Set<u32>("IsDestroyable", destroyable.IsDestroyable); //TODO: See if this could be switched to a bool
        for (size_t i = 0; i < destroyable.NumSnapPoints; i++)
        {
            ChunkSnapPoint& snapPoint = destroyable.SnapPoints[i];
            ObjectHandle snapPointObj = registry.CreateObject("", "ChunkSnapPoint");
            snapPointObj.Set<Vec3>("Position", snapPoint.Position);
            snapPointObj.Set<Mat3>("Orient", snapPoint.Orient);
        }

        chunk.AppendObjectList("Destroyables", destroyableObj);
    }

    //Load mesh
    {
        PROFILER_SCOPED("Import chunk mesh");
        ObjectHandle mesh = Importers::ImportMeshHeader(chunkMesh.MeshHeader);
        if (!mesh)
        {
            LOG_ERROR("Failed to read chunk mesh header in {}.", chunkFilename);
            return NullObjectHandle;
        }

        BinaryReader gpuFile(gpuFileBytes);
        std::optional<MeshInstanceData> meshData = chunkMesh.ReadMeshData(gpuFile);
        if (!meshData)
        {
            LOG_ERROR("Failed to read chunk mesh vertices + indices in {}.", chunkFilename);
            return NullObjectHandle;
        }

        BufferHandle indexBuffer = registry.CreateBuffer(meshData.value().IndexBuffer, fmt::format("{}_indices", Path::GetFileNameNoExtension(chunkFilename)));
        BufferHandle vertexBuffer = registry.CreateBuffer(meshData.value().VertexBuffer, fmt::format("{}_vertices", Path::GetFileNameNoExtension(chunkFilename)));
        mesh.Property("Indices").Set(indexBuffer);
        mesh.Property("Vertices").Set(vertexBuffer);
        
        chunk.Set<ObjectHandle>("Mesh", mesh);
    }

    //TODO: Properly load and assign textures. Current code is temporary just to get chunk import + render working. Really need to find material data in cchk and use that to apply the right textures to each submesh
    auto diffuseSearch = std::ranges::find_if(chunkMesh.Textures, [](const string& str) { return String::Contains(String::ToLower(str), "_d"); });
    auto specularSearch = std::ranges::find_if(chunkMesh.Textures, [](const string& str) { return String::Contains(String::ToLower(str), "_s"); });
    auto normalSearch = std::ranges::find_if(chunkMesh.Textures, [](const string& str) { return String::Contains(String::ToLower(str), "_n"); });
    if (diffuseSearch != chunkMesh.Textures.end())
        chunk.Set<ObjectHandle>("DiffuseTexture", ImportChunkTexture(*diffuseSearch, textureIndex, textureCache));
    else
        chunk.Set<ObjectHandle>("DiffuseTexture", NullObjectHandle);

    if (specularSearch != chunkMesh.Textures.end())
        chunk.Set<ObjectHandle>("SpecularTexture", ImportChunkTexture(*specularSearch, textureIndex, textureCache));
    else
        chunk.Set<ObjectHandle>("SpecularTexture", NullObjectHandle);

    if (normalSearch != chunkMesh.Textures.end())
        chunk.Set<ObjectHandle>("NormalTexture", ImportChunkTexture(*normalSearch, textureIndex, textureCache));
    else
        chunk.Set<ObjectHandle>("NormalTexture", NullObjectHandle);
    
    return chunk;
}

ObjectHandle ImportChunkTexture(std::string_view textureName, TextureIndex* textureIndex, ObjectHandle textureCache)
{
    //Check texture cache first if one was provided
    if (textureCache)
    {
        std::vector<ObjectHandle> cachedTextures = textureCache.GetObjectList("Textures");
        for (ObjectHandle cachedTexture : cachedTextures)
            if (String::EqualIgnoreCase(cachedTexture.Get<string>("Name"), textureName))
                return cachedTexture; //Already loaded. Use cached version.
    }

    //Find texture
    std::optional<string> texturePath = textureIndex->GetTexturePegPath(textureName);
    if (!texturePath)
    {
        LOG_ERROR("Failed to get texture path for chunk texture '{}'", textureName);
        return NullObjectHandle;
    }

    //Load texture
    ObjectHandle peg = Importers::ImportPegFromPath(texturePath.value(), textureIndex);
    ObjectHandle texture = GetPegEntry(peg, textureName);
    if (!texture)
    {
        LOG_ERROR("Failed to get chunk peg entry {} from {}", textureName, texturePath.value());
        return NullObjectHandle;
    }

    //Cache textures on first load
    if (textureCache)
        textureCache.AppendObjectList("Textures", texture);

    return texture;
}