using Microsoft.Extensions.DependencyInjection;

namespace Nanoforge.Misc;

public static class ServiceProviderExtensions
{
    public static T? GetService<T>(this ServiceProvider serviceProvider)
    {
        object? result = serviceProvider.GetService(typeof(T));
        return (T?)result;
    }
}