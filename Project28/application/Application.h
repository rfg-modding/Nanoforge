#pragma once
#include "common/Typedefs.h"
#include "common/timing/Timer.h"
#include <ext/WindowsWrapper.h>
#include <chrono>

struct HINSTANCE__;
typedef HINSTANCE__* HINSTANCE;

class DX11Renderer;

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

    HINSTANCE hInstance_ = nullptr;
    DX11Renderer* renderer_ = nullptr;

    int windowWidth_ = 800;
    int windowHeight_ = 800;

    MSG msg; //Windows message struct

    //Frame timing variables
    f32 deltaTime_ = 0.01f;
    const u32 maxFrameRate = 60;
    const f32 maxFrameRateDelta = 1.0f / static_cast<f32>(maxFrameRate);
};