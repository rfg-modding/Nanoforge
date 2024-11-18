using System;
using System.Linq;
using System.Reflection;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Text.Json.Serialization.Metadata;
using Nanoforge.Misc;

namespace Nanoforge.Editor;

//Resolves all types that inherit a type using reflection. Used by NanoDB during serialization and deserialization.
//This is so we don't need to put a [JsonDerivedType] on EditorObject and other bases types for every single type that inherits them.
//Stupid default behavior that's prone to human error. We have a reflection for a reason.
//More information here: https://learn.microsoft.com/en-us/dotnet/standard/serialization/system-text-json/polymorphism
public class NanoDBTypeResolver : DefaultJsonTypeInfoResolver
{
    public override JsonTypeInfo GetTypeInfo(Type type, JsonSerializerOptions options)
    {
        JsonTypeInfo jsonTypeInfo = base.GetTypeInfo(type, options);

        if (jsonTypeInfo.Type.HasBaseType<EditorObject>())
        {
            jsonTypeInfo.PolymorphismOptions = new JsonPolymorphismOptions()
            {
                //TypeDiscriminatorPropertyName = "$point-type",
                IgnoreUnrecognizedTypeDiscriminators = false,
                UnknownDerivedTypeHandling = JsonUnknownDerivedTypeHandling.FailSerialization,
                DerivedTypes = {}
            };
            var derivedTypes = Assembly.GetAssembly(typeof(EditorObject))?.GetTypes()
                .Select(type => type)
                .Where(type => jsonTypeInfo.Type.IsAssignableFrom(type)).ToArray();

            if (derivedTypes != null)
            {
                foreach (var derivedType in derivedTypes)
                {
                    jsonTypeInfo.PolymorphismOptions.DerivedTypes.Add(new JsonDerivedType(derivedType, derivedType.Name));
                }
            }

        }

        return jsonTypeInfo;
    }
}