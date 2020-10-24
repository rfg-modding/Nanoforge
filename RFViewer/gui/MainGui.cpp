#include "MainGui.h"
#include "common/Typedefs.h"
#include "render/imgui/ImGuiFontManager.h"
#include "render/imgui/imgui_ext.h"
#include "gui/panels/FileExplorer.h"
#include "gui/panels/CameraPanel.h"
#include "gui/panels/RenderSettings.h"
#include "gui/panels/ScriptxEditor.h"
#include "gui/panels/StatusBar.h"
#include "gui/panels/ZoneList.h"
#include "gui/panels/ZoneObjectsList.h"
#include "gui/panels/PropertyList.h"
#include "gui/panels/ZoneRender.h"
#include "gui/panels/LogPanel.h"
#include "Log.h"
#include <imgui/imgui.h>
#include <imgui_internal.h>


MainGui::~MainGui()
{
    node::DestroyEditor(State.NodeEditor);
}

void MainGui::Init(ImGuiFontManager* fontManager, PackfileVFS* packfileVFS, ZoneManager* zoneManager, DX11Renderer* renderer)
{
    State = GuiState{ fontManager, packfileVFS, zoneManager, renderer };

    //Create node editor library context
    State.NodeEditor = node::CreateEditor();

    //Pre-allocate gui list so we can have stable pointers to the gui
    panels_.resize(MaxGuiPanels);

    //Register all gui panels
    panels_ =
    {
        GuiPanel{&FileExplorer_Update, "Tools/File explorer", true},
        GuiPanel{&CameraPanel_Update, "Tools/Camera", true},
        GuiPanel{&RenderSettings_Update, "Tools/Render settings", true},
        GuiPanel{&StatusBar_Update, "", true},
        GuiPanel{&ZoneObjectsList_Update, "Tools/Zone objects", true},
        GuiPanel{&PropertyList_Update, "Tools/Properties", true},
        GuiPanel{&ZoneRender_Update, "", true},
        GuiPanel{&LogPanel_Update, "Tools/Log", true},
        GuiPanel{&ZoneList_Update, "Tools/Zone list", true},

        //Todo: Enable in release builds when this is a working feature
#ifdef DEBUG_BUILD
        GuiPanel{&ScriptxEditor_Update, "Tools/Scriptx editor", true},
#endif
    };

    CheckGuiListResize();
    GenerateMenus();
}

//TODO: Add scene views
//void DX11Renderer::ViewportsDoFrame()
//{
//    ////On first ever draw set the viewport size to the default one. Only happens if the viewport window doesn't have a .ini entry
//    //if (!ImGui::Begin("Scene view"))
//    //{
//    //    ImGui::End();
//    //}
//    //
//    //ImVec2 contentAreaSize;
//    //contentAreaSize.x = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;
//    //contentAreaSize.y = ImGui::GetWindowContentRegionMax().y - ImGui::GetWindowContentRegionMin().y;
//    //ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(clearColor.x, clearColor.y, clearColor.z, clearColor.w));
//
//    //Scene->Resize(contentAreaSize.x, contentAreaSize.y);
//
//    ////Render scene texture
//    //ImGui::Image(sceneViewShaderResource_, ImVec2(static_cast<f32>(sceneViewWidth_), static_cast<f32>(sceneViewHeight_)));
//    //ImGui::PopStyleColor();
//
//    //ImGui::End();
//}

void MainGui::Update(f32 deltaTime)
{
    //Draw built in / special gui elements
    DrawMainMenuBar();
    DrawDockspace();

    //Draw the rest of the gui code
    for (auto& panel : panels_)
    {
#ifdef DEBUG_BUILD
        //Todo: Add panel name to the error
        if (!panel.Update)
            THROW_EXCEPTION("Error! Update function pointer for panel was nullptr.");
#endif

        if (!panel.Open)
            continue;

        panel.Update(&State, &panel.Open);
    }
}

void MainGui::HandleResize()
{

}

void MainGui::DrawMainMenuBar()
{
    //Todo: Make this actually work
    if (ImGui::BeginMainMenuBar())
    {
        ImGuiMenu("File",
            ImGuiMenuItemShort("Open file", )
            ImGuiMenuItemShort("Save file", )
            ImGuiMenuItemShort("Exit", )
        );
        for (auto& menuItem : menuItems_)
        {
            menuItem.Draw();
        }
        ImGuiMenu("Help",
            ImGuiMenuItemShort("Welcome", )
            ImGuiMenuItemShort("Metrics", )
            ImGuiMenuItemShort("About", )
        );

        //Note: Not the preferred way of doing this with dear imgui but necessary for custom UI elements
        auto* drawList = ImGui::GetWindowDrawList();
        string framerate = std::to_string(ImGui::GetIO().Framerate);
        u64 decimal = framerate.find('.');
        const char* labelAndSeparator = "|    FPS: ";
        drawList->AddText(ImVec2(ImGui::GetCursorPosX(), 3.0f), 0xF2F5FAFF, labelAndSeparator, labelAndSeparator + strlen(labelAndSeparator));
        drawList->AddText(ImVec2(ImGui::GetCursorPosX() + 49.0f, 3.0f), ImGui::ColorConvertFloat4ToU32(gui::SecondaryTextColor), framerate.c_str(), framerate.c_str() + decimal + 3);

        ImGui::EndMainMenuBar();
    }
}

void MainGui::DrawDockspace()
{
    //Dockspace flags
    static ImGuiDockNodeFlags dockspace_flags = 0;
    
    //Parent window flags
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse
                                    | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
                                    | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetWorkPos());
    ImVec2 dockspaceSize = viewport->GetWorkSize();
    dockspaceSize.y -= State.StatusBarHeight;
    ImGui::SetNextWindowSize(dockspaceSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("DockSpace parent window", &State.Visible, window_flags);
    ImGui::PopStyleVar(3);
    
    // DockSpace
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
    {
        ImGuiID dockspace_id = ImGui::GetID("Editor dockspace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
    }
    
    ImGui::End();
}

std::vector<string> split(const string& str, const string& delim)
{
    std::vector<string> tokens;
    size_t prev = 0, pos = 0;
    do
    {
        pos = str.find(delim, prev);
        if (pos == string::npos) pos = str.length();
        string token = str.substr(prev, pos - prev);
        if (!token.empty()) tokens.push_back(token);
        prev = pos + delim.length();
    } while (pos < str.length() && prev < str.length());
    return tokens;
}

void MainGui::GenerateMenus()
{
    for (auto& panel : panels_)
    {
        //If empty then the panel is always open and doesn't have a menu entry
        if (panel.MenuPos == "")
        {
            panel.Open = true;
            continue;
        }

        //Split menu path into components
        std::vector<string> menuParts = split(panel.MenuPos, "/");
        string menuName = menuParts[0];

        //Get or create menu
        MenuItem* curMenuItem = GetMenu(menuName);
        if (!curMenuItem)
        {
            menuItems_.push_back(MenuItem{ menuName, {} });
            curMenuItem = &menuItems_.back();
        }

        
        for (int i = 1; i < menuParts.size(); i++)
        {
            string nextPart = menuParts[i];
            MenuItem* nextItem = curMenuItem->GetItem(nextPart);
            if (!nextItem)
            {
                curMenuItem->Items.push_back(MenuItem{ nextPart, {} });
                nextItem = &curMenuItem->Items.back();
            }

            curMenuItem = nextItem;
        }

        curMenuItem->panel = &panel;
    }
}

MenuItem* MainGui::GetMenu(const string& text)
{
    for (auto& item : menuItems_)
    {
        if (item.Text == text)
            return &item;
    }
    return nullptr;
}

void MainGui::CheckGuiListResize()
{
    if (panels_.capacity() != MaxGuiPanels)
        THROW_EXCEPTION("MainGui::panels_ resized! This is enforced to keep stable pointers to the gui panels. Please change MaxGuiPanels and recompile.")
}