#pragma once
#include "common/Typedefs.h"
#include "BuildConfig.h"
#include "ImGuiConfig.h"
#include "application/Config.h"
#include <IconsFontAwesome5_c.h>
#include <imgui.h>

class ImGuiFont
{
public:
    ImGuiFont(f32 size) : size_(size) {}

    void SetFont(ImFont* font) { ptr_ = font; }
    ImFont* GetFont() { return ptr_; }
    [[nodiscard]] f32 Size() const { return size_; }
    void Push() const { ImGui::PushFont(ptr_); }
    void Pop() const { ImGui::PopFont(); }
    void Load(const ImGuiIO& io, const ImFontConfig* font_cfg_template, const ImWchar* glyph_ranges, Config* config)
    {
        if (!config->Exists("UI Scale"))
        {
            config->EnsureVariableExists("UI Scale", ConfigType::Float);
            std::get<f32>(config->GetVariable("UI Scale")->Value) = 1.0f;
            config->Save();
        }

        size_ *= config->GetFloatReadonly("UI Scale").value();
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
    void Init(Config* config);
    void RegisterFonts();

    //Default font size
    ImGuiFont FontSmall{ 11.0f };
    ImGuiFont FontDefault{ 13.0f };
    ImGuiFont FontMedium{ 16.0f };

    //Fonts larger than the default font //Note: Most disabled for now since not needed yet and increases load time + mem usage
    ImGuiFont FontL{ 24.0f };
    ImGuiFont FontXL{ 31.5f };
    //ImGuiFont FontXXL(42.0f);
    //ImGuiFont FontXXXL(49.5f);
    //ImGuiFont FontXXXXL(57.0f);
    //ImGuiFont FontXXXXXL(64.5f);
    //ImGuiFont FontXXXXXXL(72.0f);

private:
    Config* config_ = nullptr;
};