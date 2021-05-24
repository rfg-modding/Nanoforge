#pragma once
#include <string>
#include "imgui.h"

namespace ImGui
{
    // ImGui::InputText() with std::string
    // Because text input needs dynamic resizing, we need to setup a callback to grow the capacity
    IMGUI_API bool InputText(const std::string& label, std::string& str, ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = NULL, void* user_data = NULL);
    IMGUI_API bool InputText(const char* label, std::string* str, ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = NULL, void* user_data = NULL);
    IMGUI_API bool InputTextMultiline(const char* label, std::string* str, const ImVec2& size = ImVec2(0, 0), ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = NULL, void* user_data = NULL);

    IMGUI_API bool Button(std::string label, const ImVec2& size_arg = ImVec2(0,0));
    IMGUI_API bool Begin(std::string name, bool* p_open = nullptr, ImGuiWindowFlags flags = 0);

    IMGUI_API void Text(std::string input);
    IMGUI_API void TextColored(const ImVec4& col, std::string input);

    IMGUI_API void OpenPopup(std::string str_id);
    IMGUI_API bool BeginPopupModal(std::string name, bool* p_open = nullptr, ImGuiWindowFlags flags = 0);
}

#define ImGuiMainMenuBar(code)\
    if(ImGui::BeginMainMenuBar())\
    {\
        code\
        ImGui::EndMainMenuBar();\
    }

#define ImGuiMenu(name, code)\
    if(ImGui::BeginMenu(name))\
    {\
        code\
        ImGui::EndMenu();\
    }

#define ImGuiMenuItem(name, shortcut, selected, enabled, code)\
    if(ImGui::MenuItem(name, shortcut, selected, enabled))\
    {\
        code\
    }

#define ImGuiMenuItemShort(name, code)\
    if(ImGui::MenuItem(name))\
    {\
        code\
    }