using Common.Math;
using Nanoforge.Gui;
using Common;
using System;

namespace ImGui
{
    extension ImGui
    {
        public static Vec4 SecondaryTextColor => .(0.32f, 0.67f, 1.0f, 1.00f);//.(0.2f, 0.7f, 1.0f, 1.00f) * 0.92f; //Light blue;
        public static Vec4 TertiaryTextColor => .(0.64f, 0.67f, 0.69f, 1.00f); //Light grey;
        public static Vec4 Red => .(0.784f, 0.094f, 0.035f, 1.0f);
        public static Vec4 Yellow => .(0.784f, 0.736f, 0.035f, 1.0f);

        [Comptime]
        private static void ComptimeChecks()
        {
            //I'm using Mirror vector types on some ImGui extension methods for convenience. These need to match up with ImGui vectors to work
            //This doesn't check that the fields are stored in the same order, but I'm not too worried about that since I think it's unlikely that either codebase will reorder x,y,z,w
            Compiler.Assert(sizeof(Vec4) == sizeof(ImGui.Vec4));
        }

        //Version of text that takes StringView & doesn't need to be null terminated
        public static void Text(StringView text)
        {
            ImGui.TextEx(text.Ptr, text.EndPtr);
        }

        public static void TextColored(StringView text, Vec4 color)
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
        public static void LabelAndValue(StringView label, StringView value, Vec4 color)
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
        		font = Fonts.FontDefault.Font;

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

        public static bool HelpMarker(StringView tooltip)
        {
            ImGui.TextDisabled("(?)");
            return ImGui.TooltipOnPrevious(tooltip);
        }

        public static bool Selectable(StringView label, bool selected = false, SelectableFlags flags = (SelectableFlags)0, Vec2 size = Vec2.Zero)
        {
            String labelNullTerminated = scope .(label)..Append('\0');
            return Selectable(labelNullTerminated.Ptr, selected, flags, size);
        }

        struct InputTextCallback_UserData
        {
            public String Buffer;
            public ImGui.InputTextCallback ChainCallback;
            public void* ChainCallbackUserData;
        }

        //Adds extra logic so we can use beef strings with ImGui.InputText
        private static int InputTextCallback(ImGui.InputTextCallbackData* data)
        {
            InputTextCallback_UserData* userData = (InputTextCallback_UserData*)data.UserData;
            if (data.EventFlag == .CallbackResize)
            {
                //Resize string + update length so the string knows that characters were added/removed
                String buffer = userData.Buffer;
                buffer.Reserve(data.BufSize);
                buffer.[Friend]mLength = data.BufTextLen;
                data.Buf = (char8*)buffer.Ptr;
            }
            if (userData.ChainCallback != null)
            {
                data.UserData = userData.ChainCallbackUserData;
                return userData.ChainCallback(data);
            }

            return 0;
        }

        public static bool InputText(StringView label, String buffer, ImGui.InputTextFlags flags = .None, ImGui.InputTextCallback callback = null, void* userData = null)
        {
            String labelNullTerminated = scope .(label)..Append('\0');

            ImGui.InputTextFlags flagsFinal = flags | .CallbackResize;
            ImGui.InputTextCallback_UserData cbUserData = .();
            cbUserData.Buffer = buffer;
            cbUserData.ChainCallback = callback;
            cbUserData.ChainCallbackUserData = userData;
            buffer.EnsureNullTerminator();
            return ImGui.InputText(labelNullTerminated.Ptr, buffer.CStr(), (u64)buffer.Length + 1, flagsFinal, => InputTextCallback, &cbUserData);
        }

        public static bool InputTextMultiline(StringView label, String buffer, ImGui.Vec2 size = .(0.0f, 0.0f), ImGui.InputTextFlags flags = .None, ImGui.InputTextCallback callback = null, void* userData = null)
        {
            String labelNullTerminated = scope .(label)..Append('\0');

            ImGui.InputTextFlags flagsFinal = flags | .CallbackResize;
            ImGui.InputTextCallback_UserData cbUserData = .();
            cbUserData.Buffer = buffer;
            cbUserData.ChainCallback = callback;
            cbUserData.ChainCallbackUserData = userData;
            buffer.EnsureNullTerminator();
            return InputTextMultiline(labelNullTerminated.Ptr, buffer.CStr(), (u64)buffer.Length + 1, size, flagsFinal, => InputTextCallback, &cbUserData);
        }

        public static bool InputTextWithHint(StringView label, StringView hint, String buffer, ImGui.InputTextFlags flags = .None, ImGui.InputTextCallback callback = null, void* userData = null)
        {
            String labelNullTerminated = scope .(label)..Append('\0');
            String hintNullTerminated = scope .(hint)..Append('\0');

            ImGui.InputTextFlags flagsFinal = flags | .CallbackResize;
            ImGui.InputTextCallback_UserData cbUserData = .();
            cbUserData.Buffer = buffer;
            cbUserData.ChainCallback = callback;
            cbUserData.ChainCallbackUserData = userData;
            buffer.EnsureNullTerminator();
            return ImGui.InputTextWithHint(labelNullTerminated.Ptr, hintNullTerminated.Ptr, buffer.CStr(), (u64)buffer.Length + 1, flagsFinal, => InputTextCallback, &cbUserData);
        }

        public static void SetClipboardText(StringView str)
        {
            String strNullTerminated = scope .(str, .NullTerminate);
            ImGui.SetClipboardText(strNullTerminated.Ptr);
        }

        public static bool Button(StringView label, ImGui.Vec2 size = .Zero)
        {
            String strNullTerminated = scope .(label, .NullTerminate);
            return ImGui.Button(strNullTerminated.Ptr, size);
        }

        public static bool ToggleButton(StringView label, bool* value, ImGui.Vec2 size = .Zero)
        {
            ImGui.PushStyleVar(.FrameBorderSize, *value ? 1.0f : 0.0f);
            ImGui.PushStyleColor(.Button, *value ? ImGui.GetColorU32(.ButtonHovered) : ImGui.GetColorU32(.WindowBg));
            bool buttonClicked = ImGui.Button(label, size);
            ImGui.PopStyleColor();
            ImGui.PopStyleVar();
            if (buttonClicked)
                *value = !(*value);

            return buttonClicked;
        }

        public struct DisposableImGuiFont : IDisposable
        {
            ImGui.Font* _font = null;
            public this(ImGui.Font* font)
            {
                _font = font;
                ImGui.PushFont(_font);
            }

            public void Dispose()
            {
                ImGui.PopFont();
            }
        }

        public static DisposableImGuiFont Font(Fonts.ImGuiFont font)
        {
            return .(font.Font);
        }

        public static mixin ScopedFont(Fonts.ImGuiFont font)
        {
            DisposableImGuiFont disposable = .(font.Font);
            defer disposable.Dispose();
        }

        public struct DisposableStyleColor : IDisposable
        {
            ImGui.Col _idx;
            ImGui.Vec4 _color;

            public this(Col idx, Vec4 color)
            {
                _idx = idx;
                _color = color;
                ImGui.PushStyleColor(idx, color);
            }

            public void Dispose()
            {
                ImGui.PopStyleColor();
            }
        }

        public static mixin ScopedStyleColor(ImGui.Col idx, Vec4 color)
        {
            DisposableStyleColor styleColor = .(idx, color);
            defer styleColor.Dispose();
        }

        extension Vec4
        {
            //Conversion from Mirror.Math.Vec4 to ImGui.Vec4
            public static operator ImGui.Vec4(Common.Math.Vec4 value)
            {
                return .(value.x, value.y, value.z, value.w);
            }
        }
    }
}