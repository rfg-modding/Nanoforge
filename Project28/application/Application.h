#pragma once
#include "common/Typedefs.h"
#include "common/timing/Timer.h"
#include <ext/WindowsWrapper.h>
#include <chrono>
#include <vector>
#include <RfgTools++\formats\packfiles\Packfile3.h>

struct HINSTANCE__;
typedef HINSTANCE__* HINSTANCE;

class DX11Renderer;
class ImGuiFontManager;

class Application
{
public:
    Application(HINSTANCE hInstance);
    ~Application();

    void Run();
    void HandleResize();

    bool Paused = false;
    Timer frameTimer_;

private:
    void InitRenderer();
    void UpdateGui();
    void ScanPackfiles();

    HINSTANCE hInstance_ = nullptr;
    DX11Renderer* renderer_ = nullptr;
    ImGuiFontManager* fontManager_ = nullptr;

    int windowWidth_ = 1600;
    int windowHeight_ = 1000;

    MSG msg; //Windows message struct

    //Frame timing variables
    f32 deltaTime_ = 0.01f;
    const u32 maxFrameRate = 60;
    const f32 maxFrameRateDelta = 1.0f / static_cast<f32>(maxFrameRate);

    std::vector<Packfile3> packfiles_ = {};

    //Todo: Make externally configurable
    std::string packfileFolderPath = "G:\\RFG Unpack\\data\\";
};