using Nanoforge.App;
using Nanoforge;
using System;
using ImGui;

namespace Nanoforge.Gui.Panels
{
	public class MainMenuBar : IGuiPanel
	{
		private ImGui.DockNodeFlags dockspaceFlags = 0;

		void IGuiPanel.Update(App app)
		{
			DrawMainMenuBar(app);
			DrawDockspace(app);
		}

		private void DrawMainMenuBar(App app)
		{
			FrameData frameData = app.GetResource<FrameData>();

			if (ImGui.BeginMainMenuBar())
			{
				if(ImGui.BeginMenu("File"))
				{
					if(ImGui.MenuItem("Open file...")) { }
					if(ImGui.MenuItem("Save file...")) { }
					if(ImGui.MenuItem("Exit")) { }
					ImGui.EndMenu();
				}
				if(ImGui.BeginMenu("Edit"))
				{
					ImGui.EndMenu();
				}
				if(ImGui.BeginMenu("View"))
				{
					//Todo: Put toggles for other guis in this layer here
					ImGui.EndMenu();
				}
				if(ImGui.BeginMenu("Help"))
				{
					if(ImGui.MenuItem("Welcome")) { }
					if(ImGui.MenuItem("Metrics")) { }
					if(ImGui.MenuItem("About")) { }
					ImGui.EndMenu();
				}

				var drawList = ImGui.GetWindowDrawList();
				String realFrameTime = scope String()..AppendF("{0:G3}", frameData.AverageFrameTime * 1000.0f);
                String totalFrameTime = scope String()..AppendF("/  {0:G4}", frameData.DeltaTime * 1000.0f);
				drawList.AddText(.(ImGui.GetCursorPosX(), 5.0f), 0xF2F5FAFF, "|    Frametime (ms): ");
				var textSize = ImGui.CalcTextSize("|    Frametime (ms): ");
				drawList.AddText(.(ImGui.GetCursorPosX() + (f32)textSize.x, 5.0f), ImGui.ColorConvertFloat4ToU32(ImGui.SecondaryTextColor), realFrameTime.CStr());
                drawList.AddText(.(ImGui.GetCursorPosX() + (f32)textSize.x + 42.0f, 5.0f), ImGui.ColorConvertFloat4ToU32(ImGui.SecondaryTextColor), totalFrameTime.CStr());

				ImGui.EndMainMenuBar();
			}
		}

		private void DrawDockspace(App app)
		{
			//Dockspace flags
			dockspaceFlags = ImGui.DockNodeFlags.None;

			//Parent window flags
			ImGui.WindowFlags windowFlags = .NoDocking | .NoTitleBar | .NoCollapse | .NoResize | .NoMove | .NoBringToFrontOnFocus | .NoNavFocus | .NoBackground;

			//Set dockspace size and params
			var viewport = ImGui.GetMainViewport();
			ImGui.SetNextWindowPos(viewport.WorkPos);
			var dockspaceSize = viewport.Size;
			ImGui.SetNextWindowSize(dockspaceSize);
			ImGui.SetNextWindowViewport(viewport.ID);
			ImGui.PushStyleVar(ImGui.StyleVar.WindowRounding, 0.0f);
			ImGui.PushStyleVar(ImGui.StyleVar.WindowBorderSize, 0.0f);
			ImGui.PushStyleVar(ImGui.StyleVar.WindowPadding, .(0.0f, 0.0f));
			ImGui.Begin("Dockspace parent window", null, windowFlags);
			ImGui.PopStyleVar(3);

			//Create dockspace
			var io = ImGui.GetIO();
			if ((io.ConfigFlags & ImGui.ConfigFlags.DockingEnable) != 0)
			{
			    ImGui.ID dockspaceId = ImGui.GetID("Editor dockspace");
			    ImGui.DockSpace(dockspaceId, .(0.0f, 0.0f), dockspaceFlags);
			}

			ImGui.End();
		}
	}
}