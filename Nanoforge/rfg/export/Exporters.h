#pragma once
#include "application/Registry.h"
#include "common/String.h"

//Functions that convert registry objects back to RFG file formats
namespace Exporters
{
	bool ExportTerritory(ObjectHandle territory, std::string_view outputPath);
	bool ExportZone(ObjectHandle zone, std::string_view outputPath, bool persistent = false);
}
