#pragma once
#include "application/Registry.h"
#include <span>

class ZoneFile;
class PackfileVFS;
class TextureIndex;
class MeshDataBlock;
class PegFile10;

//Functions that convert RFG file formats to the editor file format (registry objects)
namespace Importers
{
	ObjectHandle ImportTerritory(std::string_view territoryFilename, PackfileVFS* packfileVFS, TextureIndex* textureIndex, bool* stopSignal);
	ObjectHandle ImportZoneFile(std::span<u8> zoneBytes, std::span<u8> persistentZoneBytes, std::string_view filename, std::string_view territoryName);
	ObjectHandle ImportZoneFile(ZoneFile& zoneFile, ZoneFile& persistentZoneFile);
	//Imports both low and high lod terrain files for the given zone. They're tightly linked so it usually doesn't make sense to load them separately.
	ObjectHandle ImportTerrain(std::string_view terrainName, const Vec3& position, PackfileVFS* packfileVFS, TextureIndex* textureIndex, bool* stopSignal);
	//Imports common header used by all RFG meshes. Doesn't load index and vertex buffers. Those are handled by specific mesh importers (terrain, static, etc) since it's format specific.
	ObjectHandle ImportMeshHeader(MeshDataBlock& mesh);
	//Imports using asset path (e.g. xxxx.vpp_pc/xxxx.str2_pc/xxxx.cpeg_pc). Checks if texture was already loaded and uses that if useExistingTextures is true
	ObjectHandle ImportPegFromPath(std::string_view pegPath, TextureIndex* textureIndex, bool useExistingTextures = true);
}

//Helpers for dealing with registry objects
ObjectHandle GetZoneObject(ObjectHandle zone, std::string_view type);
PropertyHandle GetZoneProperty(ObjectHandle obj, std::string_view propertyName);
ObjectHandle GetPegEntry(ObjectHandle peg, std::string_view entryName); //Get peg entry by name. Returns NullObjectHandle if an entry with that name isn't found