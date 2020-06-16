#pragma once

class ImGuiFontManager;
class PackfileVFS;

//Todo: Split the gui out into multiple files and/or classes. Will be a mess if it's all in one file
class MainGui
{
public:
    MainGui(ImGuiFontManager* fontManager, PackfileVFS* packfileVFS) 
        : fontManager_(fontManager), packfileVFS_(packfileVFS) {}

    void Update();

    bool Visible = true;

private:
    void DrawMainMenuBar();
    void DrawDockspace();
    void DrawFileExplorer();

    ImGuiFontManager* fontManager_ = nullptr;
    PackfileVFS* packfileVFS_ = nullptr;
};