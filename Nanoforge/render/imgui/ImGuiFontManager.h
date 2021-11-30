#pragma once
#include "common/Typedefs.h"
#include "BuildConfig.h"
#include "ImGuiConfig.h"
#include "application/Config.h"
#include <IconsFontAwesome5_c.h>
#include <imgui.h>

extern CVar CVar_UIScale;

class ImGuiFont
{
public:
    ImGuiFont(f32 size) : size_(size) {}

    void SetFont(ImFont* font) { ptr_ = font; }
    ImFont* GetFont() { return ptr_; }
    [[nodiscard]] f32 Size() const { return size_; }
    void Push() const { ImGui::PushFont(ptr_); }
    void Pop() const { ImGui::PopFont(); }
    void Load(const ImGuiIO& io, const ImFontConfig* font_cfg_template, const ImWchar* glyph_ranges)
    {
        Config* config = Config::Get();

        size_ *= CVar_UIScale.Get<f32>();
        //Load normal font
        io.Fonts->AddFontFromFileTTF(BuildConfig::MainFontPath.c_str(), size_);
        //Load FontAwesome image font and merge with normal font
        ptr_ = io.Fonts->AddFontFromFileTTF(BuildConfig::FontAwesomePath.c_str(), size_, font_cfg_template, glyph_ranges);
    }

private:
    ImFont* ptr_ = nullptr;
    f32 size_ = 12.0f;
};

class ImGuiFontManager
{
public:
    void Init();
    void RegisterFonts();

    ImGuiFont FontSmall{ 11.0f };
    ImGuiFont FontDefault{ 13.0f };
    ImGuiFont FontMedium{ 16.0f };
    ImGuiFont FontL{ 24.0f };
    ImGuiFont FontXL{ 31.5f };
};