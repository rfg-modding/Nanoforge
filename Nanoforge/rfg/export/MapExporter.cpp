#include "Exporters.h"

bool Exporters::ExportTerritory(ObjectHandle territory, std::string_view outputPath, bool skipUneditedZones)
{
	for (ObjectHandle zone : territory.GetObjectList("Zones"))
	{
		if (skipUneditedZones && !zone.Get<bool>("ChildObjectEdited"))
			continue;

		if (!Exporters::ExportZone(zone, fmt::format("{}/{}", outputPath, zone.Get<string>("Name")), false)) //Generate zone file
		{
			LOG_ERROR("Failed to export territory '{}', zone '{}'", territory.Get<string>("Name"), zone.Get<string>("Name"));
			return false;
		}
		if (!Exporters::ExportZone(zone, fmt::format("{}/p_{}", outputPath, zone.Get<string>("Name")), true)) //Generate persistent zone file (same, but only has persistent objects)
		{
			LOG_ERROR("Failed to export territory '{}', persistent zone '{}'", territory.Get<string>("Name"), zone.Get<string>("Name"));
			return false;
		}
	}

	return true;
}
