using Nanoforge.Render;
using Nanoforge.Gui;
using Nanoforge.App;
using Nanoforge;
using System;
using ImGui;

namespace Nanoforge.Gui.Panels
{
	public class DebugGui : GuiPanelBase
	{
		bool ShowImGuiDemo = true;

		public override void Update(App app, Gui gui)
		{
			if (ShowImGuiDemo)
				ImGui.ShowDemoWindow(&ShowImGuiDemo);

			DrawDebugWindow(app);
		}

		private void DrawDebugWindow(App app)
		{
			if (!ImGui.Begin(scope String(Icons.ICON_FA_BUG)..Append(" Debug"), &Open))
			{
				ImGui.End();
				return;
			}

			/*Camera2D camera = app.GetResource<Camera2D>();
			RendererFrontend render = app.GetResource<RendererFrontend>();

			FontManager.FontL.Push();
			ImGui.Text(scope String(Icons.ICON_FA_BUG + " Debug"));
			FontManager.FontL.Pop();
			ImGui.Separator();

			ImGui.Checkbox("Show imgui demo", &ShowImGuiDemo);

			ImGui.SetNextItemWidth(100.0f);
			ImGui.InputFloat("Camera smoothing", &camera.CameraSmoothing);
			ImGui.SetNextItemWidth(100.0f);
			ImGui.SliderFloat("Ambient light", &render.AmbientLightIntensity, 0.0f, 1.0f);*/

			ImGui.End();
		}
	}
}