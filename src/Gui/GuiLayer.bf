using System.Collections;
using Nanoforge.App;
using Nanoforge;
using System;

namespace Nanoforge.Gui
{
	public class GuiLayer
	{
		public List<IGuiPanel> _panels ~DeleteContainerAndItems!(_panels);
		public bool Enabled;
		public readonly char8* Name;

		//Initialize with a list of gui panels that make up this layer. GuiLayer takes ownership of the list.
		public this(char8* name, List<IGuiPanel> panels, bool enabled = true)
		{
			Name = name;
			_panels = panels;
			Enabled = enabled;
		}

		//Update all panels that are part of this layer
		public void Update(App app)
		{
			for(var module in _panels)
				module.Update(app);
		}
	}
}