#pragma once

struct HINSTANCE__;
typedef HINSTANCE__* HINSTANCE;

class DX11Renderer;

class Application
{
public:
    Application(HINSTANCE hInstance);
    ~Application();

    void Run();

private:
    void InitRenderer();

    HINSTANCE hInstance_ = nullptr;
    DX11Renderer* renderer_ = nullptr;

    int windowWidth_ = 800;
    int windowHeight_ = 800;
};