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
        GuiPanel{&CameraPanel_Update, "Tools/Camera", false},
        GuiPanel{&RenderSettings_Update, "Tools/Render settings", false},
        GuiPanel{&StatusBar_Update, "", true},
        GuiPanel{&ZoneObjectsList_Update, "Tools/Zone objects", false},
        GuiPanel{&PropertyList_Update, "Tools/Properties", true},
        GuiPanel{&ZoneRender_Update, "", false},
        GuiPanel{&LogPanel_Update, "Tools/Log", true},
        GuiPanel{&ZoneList_Update, "Tools/Zone list", false},

        //Todo: Enable in release builds when this is a working feature
#ifdef DEBUG_BUILD
        GuiPanel{&ScriptxEditor_Update, "Tools/Scriptx editor", false},
#endif
    };

    CheckGuiListResize();
    GenerateMenus();
    SetThemePreset(Dark);
}

void MainGui::Update(f32 deltaTime)
{
    //Draw built in / special gui elements
    DrawMainMenuBar();
    DrawDockspace();
    ImGui::ShowDemoWindow();
    static bool firstDraw = true;

    //Draw the rest of the gui code
    for (auto& panel : panels_)
    {
        if (firstDraw)
        {
            ImGuiID dockLeftId = ImGui::DockBuilderSplitNode(dockspaceId, ImGuiDir_Left, 0.15f, nullptr, &dockspaceId);
            ImGuiID dockRightId = ImGui::DockBuilderSplitNode(dockspaceId, ImGuiDir_Right, 0.15f, nullptr, &dockspaceId);
            ImGuiID dockCentralId = ImGui::DockBuilderGetCentralNode(dockspaceId)->ID;
            ImGuiID dockCentralDownSplitId = ImGui::DockBuilderSplitNode(dockCentralId, ImGuiDir_Down, 0.20f, nullptr, &dockCentralId);

            //Todo: Tie titles to these calls so both copies don't need to be updated every time they change
            ImGui::DockBuilderDockWindow("File explorer", dockLeftId);
            ImGui::DockBuilderDockWindow("Dear ImGui Demo", dockLeftId);
            ImGui::DockBuilderDockWindow("Zones", dockLeftId);
            ImGui::DockBuilderDockWindow("Zone objects", dockLeftId);
            ImGui::DockBuilderDockWindow("Properties", dockRightId);
            ImGui::DockBuilderDockWindow("Render settings", dockRightId);
            ImGui::DockBuilderDockWindow("Scriptx editor", dockCentralId);
            ImGui::DockBuilderDockWindow("Log", dockCentralDownSplitId);

            ImGui::DockBuilderFinish(dockspaceId);
            
            firstDraw = false;
        }

        if (!panel.Open)
            continue;

        panel.Update(&State, &panel.Open);
    }

    //Draw documents
    u32 counter = 0;
    auto document = State.Documents.begin();
    while (document != State.Documents.end())
    {
        //If document is no longer open, erase it
        if (!document->Open)
        {
            if (document->OnClose)
                document->OnClose(&State, *document);
            
            document = State.Documents.erase(document);
            continue;
        }
        
        if (document->FirstDraw)
        {
            //ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderDockWindow(document->Title.c_str(), ImGui::DockBuilderGetCentralNode(dockspaceId)->ID);
            ImGui::DockBuilderFinish(dockspaceId);
        }

        //Draw the document if it's still open
        document->Update(&State, *document);
        document->FirstDraw = false;
        document++;
        counter++;
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
        ImGuiMenu("Theme",
            ImGuiMenuItemShort("Dark", SetThemePreset(Dark);)
            ImGuiMenuItemShort("Blue", SetThemePreset(Blue);)
        );
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
    
    //DockSpace
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
    {
        bool firstDraw = dockspaceId == 0;
        dockspaceId = ImGui::GetID("Editor dockspace");
        if (firstDraw)
        {
            ImGui::DockBuilderRemoveNode(dockspaceId);
            ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderFinish(dockspaceId);
        }
        ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), dockspace_flags);
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

void MainGui::SetThemePreset(ThemePreset preset)
{
    ImGuiStyle* style = &ImGui::GetStyle();
    ImVec4* colors = ImGui::GetStyle().Colors;
    
    switch (preset)
    {
    case Dark:
        //Start with imgui dark theme
        ImGui::StyleColorsDark();

        style->WindowPadding = ImVec2(8, 8);
        style->WindowRounding = 0.0f;
        style->FramePadding = ImVec2(5, 5);
        style->FrameRounding = 0.0f;
        style->ItemSpacing = ImVec2(8, 8);
        style->ItemInnerSpacing = ImVec2(8, 6);
        style->IndentSpacing = 25.0f;
        style->ScrollbarSize = 18.0f;
        style->ScrollbarRounding = 0.0f;
        style->GrabMinSize = 12.0f;
        style->GrabRounding = 0.0f;
        style->TabRounding = 0.0f;
        style->ChildRounding = 0.0f;
        style->PopupRounding = 0.0f;

        style->WindowBorderSize = 1.0f;
        style->FrameBorderSize = 1.0f;
        style->PopupBorderSize = 1.0f;

        colors[ImGuiCol_Text] = ImVec4(0.96f, 0.96f, 0.99f, 1.00f);
        colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
        colors[ImGuiCol_WindowBg] = ImVec4(0.114f, 0.114f, 0.125f, 1.0f);
        colors[ImGuiCol_ChildBg] = ImVec4(0.106f, 0.106f, 0.118f, 1.0f);
        colors[ImGuiCol_PopupBg] = ImVec4(0.06f, 0.06f, 0.07f, 1.00f);
        colors[ImGuiCol_Border] = ImVec4(0.216f, 0.216f, 0.216f, 1.0f);
        colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_FrameBg] = ImVec4(0.161f, 0.161f, 0.176f, 1.00f);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.216f, 0.216f, 0.235f, 1.00f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.255f, 0.255f, 0.275f, 1.00f);
        colors[ImGuiCol_TitleBg] = ImVec4(0.157f, 0.157f, 0.157f, 1.0f);
        colors[ImGuiCol_TitleBgActive] = ImVec4(0.216f, 0.216f, 0.216f, 1.0f);
        colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.157f, 0.157f, 0.157f, 1.0f);
        colors[ImGuiCol_MenuBarBg] = ImVec4(0.157f, 0.157f, 0.157f, 1.0f);
        colors[ImGuiCol_ScrollbarBg] = ImVec4(0.074f, 0.074f, 0.074f, 1.0f);
        colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.31f, 0.31f, 0.32f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.41f, 0.41f, 0.42f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.51f, 0.51f, 0.53f, 1.00f);
        colors[ImGuiCol_CheckMark] = ImVec4(0.44f, 0.44f, 0.47f, 1.00f);
        colors[ImGuiCol_SliderGrab] = ImVec4(0.44f, 0.44f, 0.47f, 1.00f);
        colors[ImGuiCol_SliderGrabActive] = ImVec4(0.59f, 0.59f, 0.61f, 1.00f);
        colors[ImGuiCol_Button] = ImVec4(0.20f, 0.20f, 0.22f, 1.00f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.44f, 0.44f, 0.47f, 1.00f);
        colors[ImGuiCol_ButtonActive] = ImVec4(0.59f, 0.59f, 0.61f, 1.00f);
        colors[ImGuiCol_Header] = ImVec4(0.20f, 0.20f, 0.22f, 1.00f);
        colors[ImGuiCol_HeaderHovered] = ImVec4(0.44f, 0.44f, 0.47f, 1.00f);
        colors[ImGuiCol_HeaderActive] = ImVec4(0.59f, 0.59f, 0.61f, 1.00f);
        colors[ImGuiCol_Separator] = ImVec4(1.00f, 1.00f, 1.00f, 0.20f);
        colors[ImGuiCol_SeparatorHovered] = ImVec4(0.44f, 0.44f, 0.47f, 0.39f);
        colors[ImGuiCol_SeparatorActive] = ImVec4(0.44f, 0.44f, 0.47f, 0.59f);
        colors[ImGuiCol_ResizeGrip] = ImVec4(0.26f, 0.59f, 0.98f, 0.00f);
        colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.00f);
        colors[ImGuiCol_ResizeGripActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.00f);
        colors[ImGuiCol_Tab] = ImVec4(0.21f, 0.21f, 0.24f, 1.00f);
        colors[ImGuiCol_TabHovered] = ImVec4(0.23f, 0.514f, 0.863f, 1.0f);
        colors[ImGuiCol_TabActive] = ImVec4(0.23f, 0.514f, 0.863f, 1.0f);
        colors[ImGuiCol_TabUnfocused] = ImVec4(0.21f, 0.21f, 0.24f, 1.00f);
        colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.23f, 0.514f, 0.863f, 1.0f);
        colors[ImGuiCol_DockingPreview] = ImVec4(0.23f, 0.514f, 0.863f, 0.776f);
        colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.114f, 0.114f, 0.125f, 1.0f);
        colors[ImGuiCol_PlotLines] = ImVec4(0.96f, 0.96f, 0.99f, 1.00f);
        colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.12f, 1.00f, 0.12f, 1.00f);
        colors[ImGuiCol_PlotHistogram] = ImVec4(0.96f, 0.96f, 0.99f, 1.00f);
        colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.12f, 1.00f, 0.12f, 1.00f);
        colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
        colors[ImGuiCol_DragDropTarget] = ImVec4(0.91f, 0.62f, 0.00f, 1.00f);
        colors[ImGuiCol_NavHighlight] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
        colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
        colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
        colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
        break;

    case Blue:
        //Start with imgui dark theme
        ImGui::StyleColorsDark();

        ImGui::GetStyle().FrameRounding = 4.0f;
        ImGui::GetStyle().GrabRounding = 4.0f;

        colors[ImGuiCol_Text] = ImVec4(0.95f, 0.96f, 0.98f, 1.00f);
        colors[ImGuiCol_TextDisabled] = ImVec4(0.36f, 0.42f, 0.47f, 1.00f);
        colors[ImGuiCol_WindowBg] = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
        colors[ImGuiCol_ChildBg] = ImVec4(0.15f, 0.18f, 0.22f, 1.00f);
        colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
        colors[ImGuiCol_Border] = ImVec4(0.08f, 0.10f, 0.12f, 1.00f);
        colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.278, 0.337f, 0.384f, 1.00f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.34f, 0.416f, 0.475f, 1.00f);
        colors[ImGuiCol_TitleBg] = ImVec4(0.09f, 0.12f, 0.14f, 0.65f);
        colors[ImGuiCol_TitleBgActive] = ImVec4(0.08f, 0.10f, 0.12f, 1.00f);
        colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
        colors[ImGuiCol_MenuBarBg] = ImVec4(0.15f, 0.18f, 0.22f, 1.00f);
        colors[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.39f);
        colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.278, 0.337f, 0.384f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.34f, 0.416f, 0.475f, 1.00f);
        colors[ImGuiCol_CheckMark] = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
        colors[ImGuiCol_SliderGrab] = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
        colors[ImGuiCol_SliderGrabActive] = ImVec4(0.37f, 0.61f, 1.00f, 1.00f);
        colors[ImGuiCol_Button] = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
        colors[ImGuiCol_ButtonActive] = ImVec4(0.06f, 0.53f, 0.98f, 1.00f);
        colors[ImGuiCol_Header] = ImVec4(0.20f, 0.25f, 0.29f, 0.55f);
        colors[ImGuiCol_HeaderHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
        colors[ImGuiCol_HeaderActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
        colors[ImGuiCol_Separator] = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
        colors[ImGuiCol_SeparatorHovered] = ImVec4(0.10f, 0.40f, 0.75f, 0.78f);
        colors[ImGuiCol_SeparatorActive] = ImVec4(0.10f, 0.40f, 0.75f, 1.00f);
        colors[ImGuiCol_ResizeGrip] = ImVec4(0.26f, 0.59f, 0.98f, 0.25f);
        colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
        colors[ImGuiCol_ResizeGripActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
        colors[ImGuiCol_Tab] = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
        colors[ImGuiCol_TabHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
        colors[ImGuiCol_TabActive] = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
        colors[ImGuiCol_TabUnfocused] = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
        colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
        colors[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
        colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
        colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
        colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
        colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
        colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
        colors[ImGuiCol_NavHighlight] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
        colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
        colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
        colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
        colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.15f, 0.18f, 0.22f, 1.00f);
        break;

    default:
        break;
    }
}