#pragma once

class ImGuiFontManager;
class PackfileVFS;
class Camera;

//Todo: Split the gui out into multiple files and/or classes. Will be a mess if it's all in one file
class MainGui
{
public:
    MainGui(ImGuiFontManager* fontManager, PackfileVFS* packfileVFS, Camera* camera)
        : fontManager_(fontManager), packfileVFS_(packfileVFS), camera_(camera) {}

    void Update();

    bool Visible = true;

private:
    void DrawMainMenuBar();
    void DrawDockspace();
    void DrawFileExplorer();
    void DrawCameraWindow();
    void DrawIm3dPrimitives();

    ImGuiFontManager* fontManager_ = nullptr;
    PackfileVFS* packfileVFS_ = nullptr;
    Camera* camera_ = nullptr;
};