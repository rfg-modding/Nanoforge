#include "Exporters.h"
#include <tinyxml2/tinyxml2.h>

bool Exporters::ExportTerritory(ObjectHandle territory, std::string_view outputPath, bool skipUneditedZones)
{
	for (ObjectHandle zone : territory.GetObjectList("Zones"))
	{
		if (skipUneditedZones && !zone.Get<bool>("ChildObjectEdited"))
			continue;

		//Update ParentHandle, ChildHandle, and SiblingHandle properties (in game u32 handles) with registry object handles
		//Nanoforge doesn't use the games handle system internally because it'd be inconvenient.
		for (ObjectHandle object : zone.GetObjectList("Objects"))
		{
			//Update parent handle
			ObjectHandle objParent = object.Get<ObjectHandle>("Parent");
			object.Set<u32>("ParentHandle", objParent.Valid() ? objParent.Get<u32>("Handle") : 0xFFFFFFFF);

			//Update children
			if (object.GetObjectList("Children").size() > 0) //Parent object, update children handle & update parent/sibling handles of each child
			{
				std::vector<ObjectHandle>& children = object.GetObjectList("Children");
				ObjectHandle firstChild = children[0];
				object.Set<u32>("ChildHandle", firstChild.Get<u32>("Handle"));

				for (size_t i = 0; i < children.size(); i++)
				{
					ObjectHandle child = children[i];
					ObjectHandle nextChild = (i < children.size() - 1) ? children[i + 1] : NullObjectHandle;
					
					child.Set<u32>("ParentHandle", object.Get<u32>("Handle"));
					child.Set<u32>("SiblingHandle", nextChild.Valid() ? nextChild.Get<u32>("Handle") : 0xFFFFFFFF);
				}
			}
			else //Object has no children
			{
				object.Set<u32>("ChildHandle", 0xFFFFFFFF);
			}
		}

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

bool Exporters::ExportEditorMapData(ObjectHandle territory, const string& outputFolder)
{
	tinyxml2::XMLDocument doc;
	auto* editorData = doc.NewElement("EditorData");
	doc.InsertFirstChild(editorData);
	auto* zones = editorData->InsertNewChildElement("Zones");

	//Store additional map data that typically isn't stored in RFG zone files (e.g. names, descriptions). This way it's preserved through map import/export
	for (ObjectHandle zone : territory.GetObjectList("Zones"))
	{
		auto* zoneXml = zones->InsertNewChildElement("Zone");
		zoneXml->InsertNewChildElement("Name")->SetText(zone.Get<string>("Name").c_str());
		auto* objectsXml = zoneXml->InsertNewChildElement("Objects");
		for (ObjectHandle object : zone.GetObjectList("Objects"))
		{
			auto* objectXml = objectsXml->InsertNewChildElement("Object");
			objectXml->InsertNewChildElement("Name")->SetText(object.Get<string>("Name").c_str());
			objectXml->InsertNewChildElement("Handle")->SetText(object.Get<u32>("Handle"));
			objectXml->InsertNewChildElement("Num")->SetText(object.Get<u32>("Num"));
			if (object.Has("Description"))
				objectXml->InsertNewChildElement("Description")->SetText(object.Get<string>("Description").c_str());
		}
	}

	doc.SaveFile((outputFolder + "EditorData.xml").c_str());
	return true;
}