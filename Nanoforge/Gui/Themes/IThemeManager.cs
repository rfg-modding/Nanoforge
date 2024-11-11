using Avalonia;

namespace Nanoforge.Gui.Themes;

public interface IThemeManager
{
    void Initialize(Application application);

    void Switch(int index);
}
