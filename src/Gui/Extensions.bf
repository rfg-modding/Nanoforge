using Common.Math;
using Nanoforge.Gui;
using Common;
using System;

namespace ImGui
{
    extension ImGui
    {
        public static Vec4<f32> SecondaryTextColor => .(0.32f, 0.67f, 1.0f, 1.00f);//.(0.2f, 0.7f, 1.0f, 1.00f) * 0.92f; //Light blue;
        public static Vec4<f32> TertiaryTextColor => .(0.64f, 0.67f, 0.69f, 1.00f); //Light grey;
        public static Vec4<f32> Red => .(0.784f, 0.094f, 0.035f, 1.0f);

        [Comptime]
        private static void ComptimeChecks()
        {
            //I'm using Mirror vector types on some ImGui extension methods for convenience. These need to match up with ImGui vectors to work
            //This doesn't check that the fields are stored in the same order, but I'm not too worried about that since I think it's unlikely that either codebase will reorder x,y,z,w
            Compiler.Assert(sizeof(Vec4<f32>) == sizeof(ImGui.Vec4));
        }

        //Version of text that takes StringView & doesn't need to be null terminated
        public static void Text(StringView text)
        {
            ImGui.TextEx(text.Ptr, text.EndPtr);
        }

        public static void TextColored(StringView text, Vec4<f32> color)
        {
            ImGui.PushStyleColor(.Text, .(color.x, color.y, color.z, color.w));
            ImGui.Text(text);
            ImGui.PopStyleColor();
        }

        public static bool Begin(StringView name, bool* p_open = null, WindowFlags flags = (WindowFlags)0)
        {
            String nameNullTerminated = scope $"{name}\0"; //Needs to be null terminated to work correctly since its written in C++, which uses null terminated strings
            return Begin(nameNullTerminated.Ptr, p_open, flags);
        }

        public static bool TreeNode(StringView label)
        {
            String str = scope .(label)..Append('\0');
            return ImGui.TreeNode(str.Ptr);
        }

        public static bool TreeNodeEx(StringView label, TreeNodeFlags flags = .None)
        {
            String str = scope .(label)..Append('\0');
            return ImGui.TreeNodeEx(str.Ptr, flags);
        }

        public static bool MenuItem(StringView label, char* shortcut, bool* p_selected, bool enabled = true)
        {
            String str = scope .(label)..Append('\0');
            return ImGui.MenuItem(str.Ptr, shortcut, p_selected, enabled);
        }

        public static bool BeginMenu(StringView label, bool enabled = true)
        {
            String str = scope .(label)..Append('\0');
            return ImGui.BeginMenu(str.Ptr, enabled);
        }

        public static void DockBuilderDockWindow(StringView window_name, ID node_id)
        {
            String str = scope .(window_name)..Append('\0');
            ImGui.DockBuilderDockWindow(str.Ptr, node_id);
        }

        //Draw label and value next to each other with value using secondary color
        public static void LabelAndValue(StringView label, StringView value, Vec4<f32> color)
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

        extension Vec4
        {
            //Conversion from Mirror.Math.Vec4<f32> to ImGui.Vec4
            public static operator ImGui.Vec4(Vec4<f32> value)
            {
                return .(value.x, value.y, value.z, value.w);
            }
        }
    }
}