using Common;
using System;

namespace Nanoforge.Misc
{
    [AttributeUsage(.Class | .Struct | .Interface | .Enum, .AlwaysIncludeTarget | .ReflectAttribute, ReflectUser = .All, AlwaysIncludeUser = .IncludeAllMethods | .AssumeInstantiated)]
    public struct ReflectAllAttribute : Attribute
    {

    }
}