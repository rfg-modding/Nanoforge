#pragma once
#include "application/Registry.h"

class ZoneFile;

//Functions that convert RFG file formats to the editor file format (registry objects)
namespace Importers
{
	ObjectHandle ImportZoneFile(ZoneFile& zoneFile);
}