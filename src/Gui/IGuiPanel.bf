using Nanoforge.App;
using Nanoforge;
using System;

namespace Nanoforge.Gui
{
	///A single UI window. Only one instance of each implementation should exist
	public interface IGuiPanel
	{
		//Per-frame update
		public void Update(App app, Gui gui);
	}

    ///Base class for all gui panels. Has fields and functions that all panels should have.
	///Can't do this in IGuiPanel because Beef doesn't allow interfaces to have fields like we could get away with in C++.
    public class GuiPanelBase : IGuiPanel
    {
        public bool Open = true;
        public String MenuPos = new .() ~delete _;
        public bool FirstDraw = true;

        public virtual void Update(App app, Gui gui)
        {
            return;
        }
    }
}