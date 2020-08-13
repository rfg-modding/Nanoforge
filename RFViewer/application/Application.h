#pragma once
#include "common/Typedefs.h"
#include "common/timing/Timer.h"
#include "render/camera/Camera.h"
#include "render/backend/DX11Renderer.h"
#include "render/imgui/ImGuiFontManager.h"
#include "rfg/PackfileVFS.h"
#include "gui/MainGui.h"
#include <ext/WindowsWrapper.h>
#include <chrono>

class Application
{
public:
    Application(HINSTANCE hInstance);
    ~Application();

    void Run();
    void HandleResize();

    bool Paused = false;
    Timer FrameTimer;
    Camera Camera;
    MainGui Gui;

private:
    void InitRenderer();
    void NewFrame();
    void UpdateGui();
    void LoadSettings();

    HINSTANCE hInstance_ = nullptr;
    DX11Renderer renderer_;
    ImGuiFontManager fontManager_;
    PackfileVFS packfileVFS_;

    int windowWidth_ = 1600;
    int windowHeight_ = 1000;

    //Frame timing variables
    f32 deltaTime_ = 0.01f;
    const u32 maxFrameRate = 60;
    const f32 maxFrameRateDelta = 1.0f / static_cast<f32>(maxFrameRate);

    string packfileFolderPath_ = "C:/Program Files (x86)/Steam/steamapps/common/Red Faction Guerrilla Re-MARS-tered";
};