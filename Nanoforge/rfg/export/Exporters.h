#pragma once
#include "application/Registry.h"
#include "common/String.h"

//Functions that convert registry objects back to RFG file formats
namespace Exporters
{
	bool ExportTerritory(ObjectHandle territory, std::string_view outputPath, bool skipUneditedZones = false);
	bool ExportZone(ObjectHandle zone, std::string_view outputPath, bool persistent = false);
	//Xml containing additional map data used by the editor which is stripped from the RFG zone files
	bool ExportEditorMapData(ObjectHandle territory, const string& outputFolder);
}
