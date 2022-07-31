using System.Collections;
using Nanoforge.Gui.Panels;
using Nanoforge.Math;
using Nanoforge.App;
using Nanoforge;
using System;
using ImGui;

namespace Nanoforge.Gui
{
	[System]
	public class Gui : ISystem
	{
		public List<GuiLayer> Layers = new List<GuiLayer>() ~ DeleteContainerAndItems!(Layers);

		static void ISystem.Build(App app)
		{

		}

		[SystemInit]
		void Init(App app)
		{
			//Add default debug UI elements
			GuiLayer debugLayer = GetOrCreateGuiLayer("Debug");
			debugLayer._panels.Add(new MainMenuBar());
			debugLayer._panels.Add(new DebugGui());
            debugLayer._panels.Add(new StateViewer());
			debugLayer.Enabled = true;
		}

		[SystemStage(.Update)]
		void Update(App app)
		{
			for (var layer in Layers)
				if (layer.Enabled)
					layer.Update(app);
		}

		//Destroys all gui layers and their modules
		public void Reset()
		{
			ClearAndDeleteItems(Layers);
		}

		public void EnableLayer(char8* name)
		{
			for (var layer in Layers)
				if (layer.Name == name)
					layer.Enabled = true;
		}

		public void DisableLayer(char8* name)
		{
			for (var layer in Layers)
				if (layer.Name == name)
					layer.Enabled = false;
		}

		public void ToggleLayer(char8* name)
		{
			for (var layer in Layers)
				if (layer.Name == name)
					layer.Enabled = !layer.Enabled;
		}

		public void AddGuiToLayer(char8* layerName, IGuiPanel gui)
		{
			var layer = GetOrCreateGuiLayer(layerName);
			layer.[Friend]_panels.Add(gui);
		}

		private Result<GuiLayer> GetGuiLayer(char8* layerName)
		{
			for (var layer in Layers)
				if (layer.Name == layerName)
					return layer;

			return .Err;
		}

		private GuiLayer GetOrCreateGuiLayer(char8* layerName)
		{
			var maybeLayer = GetGuiLayer(layerName);
			if (maybeLayer == .Err)
			{
				Layers.Add(new GuiLayer(layerName, new List<IGuiPanel>(), true));
				return Layers.Back;
			}
			else
			{
				return maybeLayer.Value;
			}
		}

		//Helper functions & constants
		public static Vec4<f32> SecondaryTextColor => .(0.32f, 0.67f, 1.0f, 1.00f);//.(0.2f, 0.7f, 1.0f, 1.00f) * 0.92f; //Light blue;
		public static Vec4<f32> TertiaryTextColor => .(0.64f, 0.67f, 0.69f, 1.00f); //Light grey;
		public static Vec4<f32> Red => .(0.784f, 0.094f, 0.035f, 1.0f);

		//Draw label and value next to each other with value using secondary color
		public static void LabelAndValue(StringView label, StringView value, Vec4<f32> color = SecondaryTextColor)
		{
			ImGui.Text(label);
			ImGui.SameLine();
			ImGui.SetCursorPosX(ImGui.GetCursorPosX() - 4.0f);
			ImGui.TextColored(value, color);
		}

		//Add mouse-over tooltip to previous ui element. Returns true if the target is being hovered
		public static bool TooltipOnPrevious(StringView description, ImGui.Font* Font = null)
		{
			var font = Font;
			if(font == null)
				font = FontManager.FontDefault.Font;

			bool hovered = ImGui.IsItemHovered();
		    if (hovered)
		    {
		        ImGui.PushFont(Font);
		        ImGui.BeginTooltip();
		        ImGui.PushTextWrapPos(ImGui.GetFontSize() * 35.0f);
		        ImGui.TextUnformatted(description.Ptr);
		        ImGui.PopTextWrapPos();
		        ImGui.EndTooltip();
		        ImGui.PopFont();
		    }
			return hovered;
		}
	}
}