using Nanoforge.App;
using Nanoforge;
using System;

namespace Nanoforge.Gui
{
	//A single UI window. Only one instance of each implementation should exist
	public interface IGuiPanel
	{
		//Per-frame update
		public void Update(App app);
	}
}