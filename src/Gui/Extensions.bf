using Nanoforge.Math;
using Nanoforge;
using System;

namespace ImGui
{
    extension ImGui
    {
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