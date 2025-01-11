using System.Reflection;
using AutoFixture.Kernel;

namespace Nanoforge.Tests;

public class PropertyTypeOmitter : ISpecimenBuilder
{
    Type[] _typesToOmit;

    public PropertyTypeOmitter(Type typeToOmit)
    {
        _typesToOmit = [typeToOmit];
    }
        
    public PropertyTypeOmitter(Type[] typesToOmit)
    {
        _typesToOmit = typesToOmit;
    }

    public object Create(object request, ISpecimenContext context)
    {
        var propInfo = request as PropertyInfo;
        if (propInfo != null && _typesToOmit.Contains(propInfo.PropertyType))
            return new OmitSpecimen();

        return new NoSpecimen();
    }
}