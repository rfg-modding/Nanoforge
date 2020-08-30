#pragma once
#include "common/Typedefs.h"
#include "GuiState.h"
#include "GuiBase.h"
#include <ext/WindowsWrapper.h>
#include <vector>
#include <mutex>

class ImGuiFontManager;
class PackfileVFS;
class Camera;
class ZoneManager;
class DX11Renderer;

//Todo: Split the gui out into multiple files and/or classes. Will be a mess if it's all in one file
class MainGui
{
public:
    ~MainGui();

    void Init(ImGuiFontManager* fontManager, PackfileVFS* packfileVFS, Camera* camera, ZoneManager* zoneManager, DX11Renderer* renderer);
    void Update(f32 deltaTime);
    void HandleResize();

    GuiState State;

private: 
    void DrawMainMenuBar();
    void DrawDockspace();

    std::vector<GuiPanel> panels_ = {};
};