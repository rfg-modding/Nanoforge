#include "Registry.h"
#include "common/filesystem/Path.h"
#include <ranges>
#include <string>
#include "Log.h"
#include "pugixml/src/pugixml.hpp"

Registry& Registry::Get()
{
    static Registry gRegistry;
    return gRegistry;
}

ObjectHandle Registry::CreateObject(std::string_view objectName, std::string_view typeName)
{
    _objectCreationLock.lock();
    const u64 uid = _nextUID++;
    _objects[uid] = Object(string(objectName), uid);
    _objectCreationLock.unlock();
    ObjectHandle handle = { &_objects[uid] };
    if (typeName != "")
        handle.Property("Type").Set(string(typeName));

    return handle;
}

ObjectHandle Registry::CreateObjectInternal(u64 uid, u64 parentUID, std::string_view objectName, std::string_view typeName)
{
    if (_objects.contains(uid))
        return nullptr;

    _objectCreationLock.lock();
    _objects[uid] = Object(string(objectName), uid);
    _objects[uid].ParentUID = parentUID;
    _objectCreationLock.unlock();
    ObjectHandle handle = { &_objects[uid] };
    if (typeName != "")
        handle.Property("Type").Set(string(typeName));

    return handle;
}

ObjectHandle Registry::GetObjectHandleByUID(u64 uid)
{
    if (!_objects.contains(uid))
        THROW_EXCEPTION("Failed to find object with uid '{}'", uid);

    return { &_objects[uid] };
}

u64 Registry::NumObjects() const
{
    return _objects.size();
}

bool Registry::ObjectExists(u64 uid) const
{
    auto find = _objects.find(uid);
    return find != _objects.end();
}

bool Registry::Load(const string& inFolderPath)
{
    //Clear existing state
    Registry& self = Registry::Get();
    self._objects.clear();

    string registryFilePath = inFolderPath + "\\Registry.xml";
    if (!std::filesystem::exists(registryFilePath))
        return true;

    pugi::xml_document xml;
    xml.load_file(registryFilePath.c_str());
    pugi::xml_node root = xml.child("Registry");
    if (!root)
    {
        Log->info("<Registry> block not found in registry file \"{}\"", registryFilePath);
        return false;
    }

    pugi::xml_node objects = xml.child("Objects");
    pugi::xml_node buffers = xml.child("Buffers");
    if (!objects || !buffers)
    {
        if (!objects) Log->info("<Objects> block not found in registry file \"{}\"", registryFilePath);
        if (!buffers) Log->info("<Buffers> block not found in registry file \"{}\"", registryFilePath);
        return false;
    }

    //Load objects
    {
        //Stage 1: Load objects. Set internal pointers for ObjectHandles to uidXml
        u64 highestUID = 0;
        std::vector<u64> objectsWithHandles = {}; //Objects that have ObjectHandle propertiesXml or subobjects that need patching
        pugi::xml_node objectXml = objects.child("Object");
        for (pugi::xml_node objectXml : objects.children("Object"))
        {
            //Validate object
            pugi::xml_node nameXml = objectXml.child("Name");
            pugi::xml_node typeXml = objectXml.child("Type");
            pugi::xml_node uidXml = objectXml.child("UID");
            pugi::xml_node parentUidXml = objectXml.child("ParentUID");
            pugi::xml_node propertiesXml = objectXml.child("Properties");
            pugi::xml_node subObjectsXml = objectXml.child("SubObjects");
            if (!nameXml || !uidXml || !parentUidXml || !propertiesXml || !subObjectsXml)
            {
                Log->error("Invalid registry object. Missing one or more required values: <Name>, <UID>, <ParentUID>, <Properties>, <SubObjects>. Skipping object...");
                continue;
            }

            //Create object in memory
            string name = nameXml.text().as_string();
            string objectType = typeXml ? typeXml.text().as_string() : "";
            u64 uid = uidXml.text().as_ullong();
            u64 parentUid = parentUidXml.text().as_ullong();
            ObjectHandle object = self.CreateObjectInternal(uid, parentUid, name, objectType);
            if (!object)
            {
                Log->error("Failed to create object. Name = '{}', Type = '{}', UID = {}, ParentUID = {}. Skipping object...", name, objectType, uid, parentUid);
                continue;
            }

            if (uid > highestUID)
                highestUID = uid;

            //Load properties
            for (pugi::xml_node prop : objects.children("Property"))
            {
                pugi::xml_node propNameXml = prop.child("Name");
                pugi::xml_node propTypeXml = prop.child("Type");
                pugi::xml_node propValueXml = prop.child("Value");
                if (!propNameXml || !propTypeXml || !propValueXml)
                {
                    Log->error("Invalid registry object property. Missing one or more required values: <Name>, <Type>, <Value>. Skipping property...");
                    continue;
                }
                std::string_view propName = propNameXml.text().as_string();
                std::string_view propType = propTypeXml.text().as_string();

                //Read property value
                if (propType == "i32")
                {
                    object.Property(propName).Set<i32>(propValueXml.text().as_int());
                }
                else if (propType == "u64")
                {
                    object.Property(propName).Set<u64>(propValueXml.text().as_ullong());
                }
                else if (propType == "u32")
                {
                    object.Property(propName).Set<u32>(propValueXml.text().as_uint());
                }
                else if (propType == "u16")
                {
                    object.Property(propName).Set<u16>(propValueXml.text().as_uint());
                }
                else if (propType == "u8")
                {
                    object.Property(propName).Set<u8>(propValueXml.text().as_uint());
                }
                else if (propType == "f32")
                {
                    object.Property(propName).Set<f32>(propValueXml.text().as_float());
                }
                else if (propType == "bool")
                {
                    object.Property(propName).Set<bool>(propValueXml.text().as_bool());
                }
                else if (propType == "string")
                {
                    object.Property(propName).Set<string>(propValueXml.text().as_string());
                }
                else if (propType == "ObjectHandleList")
                {
                    objectsWithHandles.push_back(uid);
                    for (pugi::xml_node handleXml : prop.children("Handle"))
                    {
                        const char* handleText = handleXml.text().as_string();
                        u64 subUid = NullUID;
                        if (handleText)
                            subUid = std::stoull(handleText);

                        object.Property(propName).GetObjectList().push_back(ObjectHandle{ (Object*)subUid });
                    }
                }
                else if (propType == "Vec3")
                {
                    Vec3 value;
                    value.x = prop.child("X").text().as_float();
                    value.y = prop.child("Y").text().as_float();
                    value.z = prop.child("Z").text().as_float();
                    object.Property(propName).Set<Vec3>(value);
                }
                else if (propType == "Mat3")
                {
                    Mat3 value;
                    value.rvec.x = prop.child("RX").text().as_float();
                    value.rvec.y = prop.child("RY").text().as_float();
                    value.rvec.z = prop.child("RZ").text().as_float();
                    value.uvec.x = prop.child("UX").text().as_float();
                    value.uvec.y = prop.child("UY").text().as_float();
                    value.uvec.z = prop.child("UZ").text().as_float();
                    value.fvec.x = prop.child("FX").text().as_float();
                    value.fvec.y = prop.child("FY").text().as_float();
                    value.fvec.z = prop.child("FZ").text().as_float();
                    object.Property(propName).Set<Mat3>(value);
                }
                else if (propType == "ObjectHandle")
                {
                    u64 handleUid = prop.text().as_ullong();
                    object.Property(propName).Set<ObjectHandle>((Object*)handleUid);
                    objectsWithHandles.push_back(uid);
                }
                else
                {
                    Log->error("Invalid registry object property. Unsupported type '{}'. Skipping property...", propType);
                    continue;
                }
            }

            //Load subobjects
            if (subObjectsXml.child("SubObject")) objectsWithHandles.push_back(uid);
            for (pugi::xml_node subObject : subObjectsXml.children("SubObject"))
            {
                u64 subObjectUid = subObject.text().as_ullong();
                object.SubObjects().push_back({ (Object*)subObjectUid });
            }
        }
        self._nextUID = highestUID + 1;

        //Stage 2: Patch ObjectHandle internal pointers
        auto patchObjectHandle = [](ObjectHandle& handle)
        {
            Registry& registry = Registry::Get();
            u64 uid = (u64)handle._object; //Stage one stores uid in handle._object since all objects aren't loaded yet
            Object* object = &registry._objects[uid];
            handle._object = object; //By stage two all objects are loaded, so we get the actual Object* and patch it
        };
        for (u64 uid : objectsWithHandles)
        {
            Object& object = self.GetObjectByUID(uid);
            //Patch properties
            for (RegistryProperty& prop : object.Properties)
            {
                if (std::holds_alternative<ObjectHandle>(prop.Value))
                {
                    patchObjectHandle(std::get<ObjectHandle>(prop.Value));
                }
                else if (std::holds_alternative<std::vector<ObjectHandle>>(prop.Value))
                {
                    std::vector<ObjectHandle>& handleList = std::get<std::vector<ObjectHandle>>(prop.Value);
                    for (ObjectHandle& handle : handleList)
                        patchObjectHandle(handle);
                }
            }

            //Patch sub object handles
            for (ObjectHandle& handle : object.SubObjects)
            {
                patchObjectHandle(handle);
            }
        }
    }

    //Load buffers
    {
        //Buffers not yet implemented
    }

    return true;
}

bool Registry::Save(const string& outFolderPath)
{
    Path::CreatePath(outFolderPath);
    Registry& registry = Registry::Get();
    pugi::xml_document xml;
    pugi::xml_node root = xml.append_child("Registry");
    pugi::xml_node objects = root.append_child("Objects");
    pugi::xml_node buffer = root.append_child("Buffers");

    //Write objects
    for (auto& kv : registry._objects)
    {
        Object& object = kv.second;
        pugi::xml_node objectXml = objects.append_child("Object");
        objectXml.append_child("Name").text().set(object.Name.c_str());
        if (object.Has("Type"))
            objectXml.append_child("Type").text().set(std::get<string>(object.Property("Type").Value).c_str());
        objectXml.append_child("UID").text().set(object.UID);
        objectXml.append_child("ParentUID").text().set(object.ParentUID);
        pugi::xml_node properties = objectXml.append_child("Properties");
        pugi::xml_node subObjects = objectXml.append_child("SubObjects");

        //Write propertiesXml
        for (RegistryProperty& prop : object.Properties)
        {
            pugi::xml_node propXml = properties.append_child("Property");
            propXml.append_child("Name").text().set(prop.Name.c_str());

            //Write property value + type string
            {
                pugi::xml_node propTypeXml = propXml.append_child("Type");
                pugi::xml_node propValueXml = propXml.append_child("Value");

                string propType = "";
                if (std::holds_alternative<i32>(prop.Value))
                {
                    propType = "i32";
                    propValueXml.text().set(std::get<i32>(prop.Value));
                }
                else if (std::holds_alternative<u64>(prop.Value))
                {
                    propType = "u64";
                    propValueXml.text().set(std::get<u64>(prop.Value));
                }
                else if (std::holds_alternative<u32>(prop.Value))
                {
                    propType = "u32";
                    propValueXml.text().set(std::get<u32>(prop.Value));
                }
                else if (std::holds_alternative<u16>(prop.Value))
                {
                    propType = "u16";
                    propValueXml.text().set(std::get<u16>(prop.Value));
                }
                else if (std::holds_alternative<u8>(prop.Value))
                {
                    propType = "u8";
                    propValueXml.text().set(std::get<u8>(prop.Value));
                }
                else if (std::holds_alternative<f32>(prop.Value))
                {
                    propType = "f32";
                    propValueXml.text().set(std::get<f32>(prop.Value));
                }
                else if (std::holds_alternative<bool>(prop.Value))
                {
                    propType = "bool";
                    propValueXml.text().set(std::get<bool>(prop.Value));
                }
                else if (std::holds_alternative<string>(prop.Value))
                {
                    propType = "string";
                    propValueXml.text().set(std::get<string>(prop.Value).c_str());
                }
                else if (std::holds_alternative<std::vector<ObjectHandle>>(prop.Value))
                {
                    propType = "ObjectHandleList";
                    std::vector<ObjectHandle>& handles = std::get<std::vector<ObjectHandle>>(prop.Value);
                    for (ObjectHandle handle : handles)
                    {
                        pugi::xml_node handleXml = propValueXml.append_child("Handle");
                        handleXml.text().set(handle.UID());
                    }
                }
                else if (std::holds_alternative<Vec3>(prop.Value))
                {
                    propType = "Vec3";
                    Vec3& value = std::get<Vec3>(prop.Value);
                    propValueXml.append_child("Y").text().set(value.y);
                    propValueXml.append_child("Z").text().set(value.z);
                    propValueXml.append_child("X").text().set(value.x);
                }
                else if (std::holds_alternative<Mat3>(prop.Value))
                {
                    propType = "Mat3";
                    Mat3& value = std::get<Mat3>(prop.Value);
                    propValueXml.append_child("RX").text().set(value.rvec.x);
                    propValueXml.append_child("RY").text().set(value.rvec.y);
                    propValueXml.append_child("RZ").text().set(value.rvec.z);
                    propValueXml.append_child("UX").text().set(value.uvec.x);
                    propValueXml.append_child("UY").text().set(value.uvec.y);
                    propValueXml.append_child("UZ").text().set(value.uvec.z);
                    propValueXml.append_child("FX").text().set(value.fvec.x);
                    propValueXml.append_child("FY").text().set(value.fvec.y);
                    propValueXml.append_child("FZ").text().set(value.fvec.z);
                }
                else if (std::holds_alternative<ObjectHandle>(prop.Value))
                {
                    propType = "ObjectHandle";
                    propValueXml.text().set(std::get<ObjectHandle>(prop.Value).UID());
                }
                else
                {
                    Log->error("Unsupported PropertyValue type for property '{}', object uid = {}", prop.Name, object.UID);
                    return false;
                }
                propTypeXml.text().set(propType.c_str());
            }
        }

        //Write sub objects
        for (ObjectHandle subObject : object.SubObjects)
            subObjects.append_child("SubObject").text().set(subObject.UID());
    }

    //Write buffers
    {
        //Buffers not yet implemented
    }

    string outXmlPath = outFolderPath + "\\Registry.xml";
    xml.save_file(outXmlPath.c_str());
    return true;
}

Object& Registry::GetObjectByUID(u64 uid)
{
    if (!_objects.contains(uid))
        THROW_EXCEPTION("Failed to find object with uid '{}'", uid);

    return _objects[uid];
}

ObjectHandle::ObjectHandle(Object* object) : _object(object)
{

}

u64 ObjectHandle::UID() const
{
    return _object ? _object->UID : NullUID;
}

bool ObjectHandle::Valid() const
{
    return _object && _object->UID != NullUID;
}

//Allows checking handle validity with if statements. E.g. if (handle) { /*valid*/ } else { /*invalid*/ }
ObjectHandle::operator bool() const
{
    return Valid();
}

std::vector<ObjectHandle>& ObjectHandle::SubObjects()
{
    if (_object)
        return _object->SubObjects;
    else
        THROW_EXCEPTION("Attempted to get SubObjects from null object handle.")
}

PropertyHandle ObjectHandle::Property(std::string_view name)
{
    if (!_object)
        THROW_EXCEPTION("Attempted to get property from a null object.");

    //Search for property
    for (size_t i = 0; i < _object->Properties.size(); i++)
        if (_object->Properties[i].Name == name)
            return { _object, (u32)i }; //Property found

    //Property not found. Add it to the object and return its handle
    RegistryProperty& prop = _object->Properties.emplace_back();
    prop.Name = string(name);
    return { _object, (u32)(_object->Properties.size() - 1) };
}

bool ObjectHandle::Has(std::string_view propertyName)
{
    if (_object)
        for (size_t i = 0; i < _object->Properties.size(); i++)
            if (_object->Properties[i].Name == propertyName)
                return true;

    return false;
}

std::vector<PropertyHandle> ObjectHandle::Properties()
{
    std::vector<PropertyHandle> properties = {};
    for (u32 i = 0; i < _object->Properties.size(); i++)
        properties.emplace_back(_object, i);

    return properties;
}

bool ObjectHandle::operator==(ObjectHandle other)
{
    return _object == other._object;
}

RegistryProperty& Object::Property(std::string_view name)
{
    //Search for property
    for (RegistryProperty& prop : Properties)
        if (prop.Name == name)
            return prop;

    //Property not found. Add it to the object
    RegistryProperty& prop = Properties.emplace_back();
    prop.Name = string(name);
    return prop;
}

bool Object::Has(std::string_view propertyName)
{
    for (RegistryProperty& prop : Properties)
        if (prop.Name == propertyName)
            return true;

    return false;
}
