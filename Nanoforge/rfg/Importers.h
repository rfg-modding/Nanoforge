#pragma once
#include "application/Registry.h"

class ZoneFile;

//Functions that convert RFG file formats to the editor file format (registry objects)
namespace Importers
{
	ObjectHandle ImportZoneFile(ZoneFile& zoneFile);
}

//Helpers for dealing with zone related registry objects
ObjectHandle GetZoneObject(ObjectHandle zone, std::string_view classname);
ObjectHandle GetZoneProperty(ObjectHandle obj, std::string_view propertyName);