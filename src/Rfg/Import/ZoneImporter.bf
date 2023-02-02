using RfgTools.Formats.Zones;
using Nanoforge.App.Project;
using System.Diagnostics;
using System.Collections;
using Nanoforge.Misc;
using Common.Math;
using Common;
using System;
using RfgTools.Types;

namespace Nanoforge.Rfg.Import
{
	public static class ZoneImporter
	{
        private static append Dictionary<StringView, ZoneObjectReaderFunc> _zoneObjectReaders;

        //Typed object readers. preExisting can be null to indicate that the reader should create a new instance.
        //When non null it's being used to read inherited properties. E.g. ItemReader calls ObjectReader since item inherits object and object properties.
        typealias ZoneObjectReaderFunc = function Result<ZoneObject, StringView>(ZoneObject preExisting, RfgZoneObject* rfgObj, Dictionary<ZoneObject, ObjectRelationsTuple> relatives, DiffUtil changes);
        typealias ObjectRelationsTuple = (u32 parentHandle, u32 firstChildHandle, u32 siblingHandle);

        public static this()
        {
            //Initialize zone object readers
            _zoneObjectReaders["object"] = => ObjectReader;
            _zoneObjectReaders["obj_zone"] = => ObjZoneReader;
            _zoneObjectReaders["navpoint"] = => NavPointReader;
            _zoneObjectReaders["cover_node"] = => CoverNodeReader;
        }

        public static Result<Zone, StringView> ImportZone(Span<u8> fileBytes, Span<u8> persistentFileBytes, StringView zoneFilename, DiffUtil changes)
        {
            //Parse zone files
            ZoneFile36 zoneFile = scope .();
            if (zoneFile.Load(fileBytes, useInPlace: true) case .Err(let err))
            {
                Log.Error(scope $"Failed to parse zone file '{zoneFilename}'. {err}");
                return .Err("Failed to parse zone file");
            }
            ZoneFile36 persistentZoneFile = scope .();
            if (persistentZoneFile.Load(persistentFileBytes, useInPlace: true) case .Err(let err)) //Note: useInPlace is true so the Span<u8> must stay alive as long as the ZoneFile is alive
            {
                Log.Error(scope $"Failed to parse persistent zone file 'p_{zoneFilename}'. {err}");
                return .Err("Failed to parse persistent zone file");
            }

            //Convert zone file to an editor object
            Zone zone = changes.CreateObject<Zone>(zoneFilename);
            zone.District.Set(zoneFile.DistrictName);
            zone.DistrictFlags = zoneFile.Header.DistrictFlags;
            zone.ActivityLayer = false;
            zone.MissionLayer = false;

            Dictionary<ZoneObject, ObjectRelationsTuple> relatives = scope .();
            for (RfgZoneObject* rfgObj in zoneFile.Objects)
            {
                if (_zoneObjectReaders.ContainsKey(rfgObj.Classname))
                {
                    switch (_zoneObjectReaders[rfgObj.Classname](null, rfgObj, relatives, changes))
                    {
                        case .Ok(let obj):
                            zone.Objects.Add(obj);
                        case .Err(let err):
                            Log.Error(scope $"Zone object reader for class '{rfgObj.Classname}' failed. {err}");
                            return .Err("Zone object read error.");
                    }
                }
                else
                {
                    Log.Error(scope $"Encountered an unsupported zone object class '{rfgObj.Classname}'. Please write an importer for that class and try again.");
                    return .Err("Encountered an unsupported zone object class.");
                }
            }

            //TODO: Add some sanity checks in another function:
            //         - Check for unknown/unsupported properties & object classes
            //         - Check for objects which are their own parent or sibling
            //         - Check for loops in parent/child or sibling linked lists
            //         - Check for parent/sibling/child references to non-existent objects
            //         - Return error enums from RfgZoneObject.GetProperty() functions.
            //               - Useful to know if a property just wasn't found or if it's present but has the wrong type or size. Map import should halt in case of corrupt/poorly formed data.
            //         - Benchmark these on all MP + WC zones. If it takes a significant amount of time make it toggleable
            //TODO: Implement the remaining object class importers used by multiplayer and wrecking crew maps.

            //Mark persistent objects by finding matching objects in the persistent zone file
            for (RfgZoneObject* rfgObj in persistentZoneFile.Objects)
            {
                for (ZoneObject obj in zone.Objects)
                {
                    if (obj.Handle == rfgObj.Handle && obj.Num == rfgObj.Num)
                    {
#if DEBUG
                        if (!StringView.Equals(obj.Classname, rfgObj.Classname, true))
                        {
                            Log.Error(scope $"Object type mismatch between primary and persistent zone files for {zone.Name}");
                            return .Err("Object type mismatch between primary and persistent zone files");
                        }
#endif

                        obj.Persistent = true;
                        break;
                    }
                }
            }

            //Set relative references
            for (var kv in relatives)
            {
                ZoneObject primaryObj = kv.key;
                (u32 parentHandle, u32 firstChildHandle, u32 siblingHandle) = kv.value;

                //Find parent
                if (parentHandle != RfgZoneObject.InvalidHandle)
                {
                    for (ZoneObject secondaryObj in zone.Objects)
                    {
                        if (secondaryObj.Handle == parentHandle)
                        {
                            primaryObj.Parent = secondaryObj;
                            break;
                        }
                    }
                }

                //Find first child
                if (firstChildHandle != RfgZoneObject.InvalidHandle)
                {
                    ZoneObject child = null;
                    for (ZoneObject secondaryObj in zone.Objects)
                    {
                        if (secondaryObj.Handle == firstChildHandle)
                        {
                            primaryObj.Children.Add(child);
                            child = secondaryObj;
                            break;
                        }
                    }

                    //Fill children list
                    while (child != null)
                    {
                        if (!relatives.ContainsKey(child))
                            break;

                        (u32 _, u32 __, u32 childSiblingHandle) = relatives[child];
                        if (childSiblingHandle == RfgZoneObject.InvalidHandle)
                            break;

                        for (ZoneObject secondaryObj in zone.Objects)
                        {
                            if (secondaryObj.Handle == childSiblingHandle)
                            {
                                primaryObj.Children.Add(secondaryObj);
                                child = secondaryObj;
                            }
                        }
                    }
                }
            }

            //TODO: Determine user friendly name based on properties. Maybe fold that into the typed object loaders. See NF++ for property list + concatenation rules

            return .Ok(zone);
        }

        private static mixin CreateObject<T>(ZoneObject preExisting, DiffUtil changes) where T : ZoneObject
        {
            preExisting != null ? (T)preExisting : changes.CreateObject<T>()
        }

        //object - base class inherited by all other zone object classes
        private static Result<ZoneObject, StringView> ObjectReader(ZoneObject preExisting, RfgZoneObject* rfgObj, Dictionary<ZoneObject, ObjectRelationsTuple> relatives, DiffUtil changes)
        {
            ZoneObject obj = CreateObject!<ZoneObject>(preExisting, changes);
            obj.Classname.Set(rfgObj.Classname);
            obj.Handle = rfgObj.Handle;
            obj.Num = rfgObj.Num;
            obj.Flags = (ZoneObject.Flags)rfgObj.Flags;
            obj.Bmin = rfgObj.Bmin;
            obj.Bmax = rfgObj.Bmax;
            relatives[obj] = (rfgObj.Parent, rfgObj.Child, rfgObj.Sibling); //Store relations in temporary dictionary. They get applied to the related properties once all objects are loaded

            //Position & orient
            if (rfgObj.GetVec3("just_pos") case .Ok(Vec3<f32> val))
            {
                obj.Position = val;
                obj.Orient = .Identity;
            }
            else if (rfgObj.GetPositionOrient("op") case .Ok(PositionOrient val)) //Only check op if just_pos is missing
            {
                obj.Position = val.Position;
                obj.Orient = val.Orient;
            }
            else
            {
                //Position is missing. Set to the center of the bounding box. Which all objects have as part of the zone format
                Vec3<f32> bboxSize = obj.Bmax - obj.Bmin;
                obj.Position = obj.Bmin + (bboxSize / 2.0f);
                obj.Orient = .Identity;
            }

            //Optional display name. No vanilla objects have this
            if (rfgObj.GetString("display_name") case .Ok(StringView val))
            {
                obj.SetName(val);
            }

            return obj;
        }

        //obj_zone : object
        private static Result<ZoneObject, StringView> ObjZoneReader(ZoneObject preExisting, RfgZoneObject* rfgObj, Dictionary<ZoneObject, ObjectRelationsTuple> relatives, DiffUtil changes)
        {
            ObjZone obj = CreateObject!<ObjZone>(preExisting, changes);
            if (ObjectReader(obj, rfgObj, relatives, changes) case .Err(let err)) //Read inherited properties
                return .Err(err);

            if (rfgObj.GetString("ambient_spawn") case .Ok(StringView val))
                obj.AmbientSpawn.Set(val);
            if (rfgObj.GetString("spawn_resource") case .Ok(StringView val))
                obj.SpawnResource.Set(val);
            if (rfgObj.GetString("terrain_file_name") case .Ok(StringView val))
                obj.TerrainFileName.Set(val);
            if (rfgObj.GetF32("wind_min_speed") case .Ok(f32 val))
                obj.WindMinSpeed = val;
            if (rfgObj.GetF32("wind_max_speed") case .Ok(f32 val))
                obj.WindMaxSpeed = val;

            return obj;
        }

        //navpoint : object
        private static Result<ZoneObject, StringView> NavPointReader(ZoneObject preExisting, RfgZoneObject* rfgObj, Dictionary<ZoneObject, ObjectRelationsTuple> relatives, DiffUtil changes)
        {
            NavPoint obj = CreateObject!<NavPoint>(preExisting, changes);
            if (ObjectReader(obj, rfgObj, relatives, changes) case .Err(let err)) //Read inherited properties
                return .Err(err);

            //TODO: Read nav node specific properties. Not needed until SP map editing is added.

            return obj;
        }

        //cover_node : object
        private static Result<ZoneObject, StringView> CoverNodeReader(ZoneObject preExisting, RfgZoneObject* rfgObj, Dictionary<ZoneObject, ObjectRelationsTuple> relatives, DiffUtil changes)
        {
            CoverNode obj = CreateObject!<CoverNode>(preExisting, changes);
            if (ObjectReader(obj, rfgObj, relatives, changes) case .Err(let err)) //Read inherited properties
                return .Err(err);

            //TODO: Read cover node specific properties. Not needed until SP map editing is added.

            return obj;
        }
	}
}