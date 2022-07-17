using Nanoforge;
using System;

namespace Nanoforge.Misc
{
    [AttributeUsage(.Class | .Interface, .AlwaysIncludeTarget | .ReflectAttribute, ReflectUser = .All, AlwaysIncludeUser = .IncludeAllMethods | .AssumeInstantiated)]
    public struct ReflectAllAttribute : Attribute
    {

    }
}