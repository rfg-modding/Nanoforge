#include "MainGui.h"
#include "common/Typedefs.h"
#include "render/imgui/ImGuiFontManager.h"
#include "render/imgui/imgui_ext.h"
#include "render/imgui/ImGuiConfig.h"
#include "gui/panels/file_explorer/FileExplorer.h"
#include "gui/panels/scriptx_editor/ScriptxEditor.h"
#include "gui/panels/StatusBar.h"
#include "gui/panels/StartPanel.h"
#include "gui/panels/ZoneList.h"
#include "gui/panels/ZoneObjectsList.h"
#include "gui/panels/property_panel/PropertyPanel.h"
#include "gui/panels/LogPanel.h"
#include "application/project/Project.h"
#include "gui/documents/TerritoryDocument.h"
#include "Log.h"
#include "gui/misc/SettingsGui.h"
#include "application/Config.h"
#include "gui/util/WinUtil.h"
#include "util/RfgUtil.h"
#include "render/backend/DX11Renderer.h"
#include "gui/util/HelperGuis.h"
#include <imgui/imgui.h>
#include <imgui_internal.h>
#include <spdlog/fmt/fmt.h>
#include "gui/documents/LocalizationDocument.h"

//Used in MainGui::DrawMainMenuBar()
std::vector<const char*> TerritoryList =
{
    "terr01",
    "dlc01",
    "mp_cornered",
    "mp_crashsite",
    "mp_crescent",
    "mp_crevice",
    "mp_deadzone",
    "mp_downfall",
    "mp_excavation",
    "mp_fallfactor",
    "mp_framework",
    "mp_garrison",
    "mp_gauntlet",
    "mp_overpass",
    "mp_pcx_assembly",
    "mp_pcx_crossover",
    "mp_pinnacle",
    "mp_quarantine",
    "mp_radial",
    "mp_rift",
    "mp_sandpit",
    "mp_settlement",
    "mp_warlords",
    "mp_wasteland",
    "mp_wreckage",
    "mpdlc_broadside",
    "mpdlc_division",
    "mpdlc_islands",
    "mpdlc_landbridge",
    "mpdlc_minibase",
    "mpdlc_overhang",
    "mpdlc_puncture",
    "mpdlc_ruins",
    "wc1",
    "wc2",
    "wc3",
    "wc4",
    "wc5",
    "wc6",
    "wc7",
    "wc8",
    "wc9",
    "wc10",
    "wcdlc1",
    "wcdlc2",
    "wcdlc3",
    "wcdlc4",
    "wcdlc5",
    "wcdlc6",
    "wcdlc7",
    "wcdlc8",
    "wcdlc9"
};

void MainGui::Init(ImGuiFontManager* fontManager, PackfileVFS* packfileVFS, DX11Renderer* renderer, Project* project, XtblManager* xtblManager, Config* config, Localization* localization)
{
    TRACE();
    State = GuiState{ fontManager, packfileVFS, renderer, project, xtblManager, config, localization };

    //Ensure that values used by the UI exist
    State.Config->EnsureVariableExists("Show FPS", ConfigType::Bool);
    State.Config->EnsureVariableExists("UI Scale", ConfigType::Float);
    State.Config->EnsureVariableExists("Recent projects", ConfigType::List);

    //Create all gui panels
    AddMenuItem("", true, CreateHandle<StatusBar>());
    AddMenuItem("View/Start page", true, CreateHandle<StartPanel>());
    AddMenuItem("View/Properties", true, CreateHandle<PropertyPanel>());
    AddMenuItem("View/Log", true, CreateHandle<LogPanel> ());
    AddMenuItem("View/Zone objects", true, CreateHandle<ZoneObjectsList>());
    AddMenuItem("View/Zone list", true, CreateHandle<ZoneList>());
    AddMenuItem("View/File explorer", true, CreateHandle<FileExplorer>());
    AddMenuItem("View/Scriptx viewer (WIP)", false, CreateHandle<ScriptxEditor>(&State));

    GenerateMenus();
    gui::SetThemePreset(Dark);
}

void MainGui::Update(f32 deltaTime)
{
    //Draw always visible UI elements
    DrawMainMenuBar();
    DrawDockspace();
#ifdef DEBUG_BUILD
    ImGui::ShowDemoWindow();
#endif

    //Draw panels
    for (auto& panel : panels_)
    {
        if (!panel->Open)
            continue;

        panel->Update(&State, &panel->Open);
        panel->FirstDraw = false;
    }

    //Move newly created documents into main vector. Done this way to avoid iterator invalidation when documents are created by other documents
    for (auto& doc : State.NewDocuments)
    {
        State.Documents.push_back(doc);
    }
    State.NewDocuments.clear();

    //Draw documents. Always docked to the center node of the dockspace when opened.
    auto iter = State.Documents.begin();
    while (iter != State.Documents.end())
    {
        Handle<IDocument> document = *iter;
        if (document->FirstDraw)
        {
            ImGui::DockBuilderDockWindow(document->Title.c_str(), ImGui::DockBuilderGetCentralNode(dockspaceId)->ID);
            ImGui::DockBuilderFinish(dockspaceId);
            document->FirstDraw = false;
        }

        if (document->Open())
        {
            document->Update(&State);
            iter++;
        }
        else
        {
            iter = State.Documents.erase(iter);
        }
    }

    //Draw mod packaging modal
    if (showModPackagingPopup_)
        DrawModPackagingPopup(&showModPackagingPopup_, &State);
    if (showNewProjectWindow_)
        DrawNewProjectWindow(&showNewProjectWindow_, State.CurrentProject, State.Config);
    if (showSettingsWindow_)
        DrawSettingsGui(&showSettingsWindow_, State.Config, State.FontManager);
}

void MainGui::AddMenuItem(string menuPos, bool open, Handle<IGuiPanel> panel)
{
    //Make sure panel with same menu position doesn't already exist
    for (auto& guiPanel : panels_)
    {
        if (String::EqualIgnoreCase(guiPanel->MenuPos, menuPos))
        {
            return;
        }
    }

    //Add panel to list
    panel->MenuPos = menuPos;
    panel->Open = open;
    panels_.emplace_back(panel);
}

void MainGui::DrawMainMenuBar()
{
    //Todo: Make this actually work
    if (ImGui::BeginMainMenuBar())
    {
        //Draw file menu
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("New project..."))
            {
                showNewProjectWindow_ = true;
            }
            if (ImGui::MenuItem("Open project..."))
            {
                TryOpenProject(State.CurrentProject, State.Config);
            }
            if (ImGui::MenuItem("Save project", nullptr, false, State.CurrentProject->Loaded()))
            {
                State.CurrentProject->Save();
            }
            if (ImGui::MenuItem("Close project", nullptr, false, State.CurrentProject->Loaded()))
            {
                State.CurrentProject->Close();
            }
            if (ImGui::BeginMenu("Recent projects"))
            {
                auto recentProjects = State.Config->GetListReadonly("Recent projects").value();
                for (auto& path : recentProjects)
                {
                    if (ImGui::MenuItem(Path::GetFileName(path).c_str()))
                    {
                        if (State.CurrentProject->Load(path))
                        {
                            ImGui::End();
                            return;
                        }
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::Separator();

            if (ImGui::MenuItem("Package mod...", nullptr, false, State.CurrentProject->Loaded()))
            {
                showModPackagingPopup_ = true;
                State.CurrentProject->WorkerFinished = false;
            }
            ImGui::Separator();

            if (ImGui::MenuItem("Settings"))
            {
                showSettingsWindow_ = true;
            }
            ImGui::Separator();

            if (ImGui::MenuItem("Exit"))
            {
                exit(EXIT_SUCCESS);
            }
            ImGui::EndMenu();
        }

        //Draw menu item for each panel (e.g. file explorer, properties, log, etc)
        for (auto& menuItem : menuItems_)
        {
            menuItem.Draw();
        }

        //Draw tools menu
        if (ImGui::BeginMenu("Tools"))
        {
            if (ImGui::BeginMenu("Open territory"))
            {
                for (const char* territory : TerritoryList)
                {
                    if (ImGui::MenuItem(territory))
                    {
                        string territoryName = string(territory);
                        State.SetTerritory(territoryName);
                        State.CreateDocument(territoryName, CreateHandle<TerritoryDocument>(&State, State.CurrentTerritoryName, State.CurrentTerritoryShortname));
                    }
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Localization"))
            {
                if (ImGui::MenuItem("View localized strings"))
                {
                    State.CreateDocument("Localization", CreateHandle<LocalizationDocument>(&State));
                }
                if (ImGui::BeginMenu("Locale"))
                {
                    for (auto& localeClass : State.Localization->Classes)
                    {
                        bool selected = localeClass.Type == State.Localization->CurrentLocale;
                        if (ImGui::MenuItem(localeClass.Name.c_str(), nullptr, &selected))
                        {
                            State.Localization->CurrentLocale = localeClass.Type;
                        }
                    }

                    ImGui::EndMenu();
                }

                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }

        //Draw themes menu
        if (ImGui::BeginMenu("Theme"))
        {
            if (ImGui::MenuItem("Dark"))
            {
                gui::SetThemePreset(Dark);
            }
            if (ImGui::MenuItem("Blue"))
            {
                gui::SetThemePreset(Blue);
            }
            ImGui::EndMenu();
        }

        bool showFPS = State.Config->GetBoolReadonly("Show FPS").value();
        f32 uiScale = State.Config->GetFloatReadonly("UI Scale").value();
        ImVec2 cursorPos = { ImGui::GetCursorPosX(), 3.0f };
        auto* drawList = ImGui::GetWindowDrawList();

        //Draw project name
        string projectName = State.CurrentProject->Loaded() ? State.CurrentProject->Name : "No project loaded";
        projectName = "|    " + projectName;
        drawList->AddText(cursorPos, 0xF2F5FAFF, projectName.c_str(), projectName.c_str() + projectName.length());
        cursorPos.x += ImGui::CalcTextSize(projectName.c_str()).x;

        //Draw FPS meter if it's enabled
        if (showFPS)
        {
            string framerate = std::to_string(ImGui::GetIO().Framerate);
            u64 decimal = framerate.find('.');
            const char* labelAndSeparator = "    |    FPS: ";

            //FPS label
            drawList->AddText(cursorPos, 0xF2F5FAFF, labelAndSeparator, labelAndSeparator + strlen(labelAndSeparator));
            cursorPos.x += ImGui::CalcTextSize(labelAndSeparator).x;

            //FPS value
            drawList->AddText(cursorPos, ImGui::ColorConvertFloat4ToU32(gui::SecondaryTextColor), framerate.c_str(), framerate.c_str() + decimal + 3);
            cursorPos.x += ImGui::CalcTextSize(framerate.c_str(), framerate.c_str() + decimal + 3).x;
        }

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
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImVec2 dockspaceSize = viewport->WorkSize;
    dockspaceSize.y -= State.StatusBarHeight;
    ImGui::SetNextWindowSize(dockspaceSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("DockSpace parent window", nullptr, window_flags);
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

    //Set default docking positions on first draw
    static bool firstDraw = true;
    if (firstDraw)
    {
        ImGuiID dockLeftId = ImGui::DockBuilderSplitNode(dockspaceId, ImGuiDir_Left, 0.15f, nullptr, &dockspaceId);
        ImGuiID dockLeftBottomId = ImGui::DockBuilderSplitNode(dockLeftId, ImGuiDir_Down, 0.5f, nullptr, &dockLeftId);
        ImGuiID dockRightId = ImGui::DockBuilderSplitNode(dockspaceId, ImGuiDir_Right, 0.15f, nullptr, &dockspaceId);
        ImGuiID dockCentralId = ImGui::DockBuilderGetCentralNode(dockspaceId)->ID;
        ImGuiID dockCentralDownSplitId = ImGui::DockBuilderSplitNode(dockCentralId, ImGuiDir_Down, 0.20f, nullptr, &dockCentralId);

        //Todo: Tie titles to these calls so both copies don't need to be updated every time they change
        ImGui::DockBuilderDockWindow("Start page", dockCentralId);
        ImGui::DockBuilderDockWindow("File explorer", dockLeftId);
        ImGui::DockBuilderDockWindow("Dear ImGui Demo", dockLeftId);
        ImGui::DockBuilderDockWindow("Zones", dockLeftId);
        ImGui::DockBuilderDockWindow("Zone objects", dockLeftBottomId);
        ImGui::DockBuilderDockWindow("Properties", dockRightId);
        ImGui::DockBuilderDockWindow("Render settings", dockRightId);
        ImGui::DockBuilderDockWindow("Scriptx viewer", dockCentralId);
        ImGui::DockBuilderDockWindow("Log", dockCentralDownSplitId);

        ImGui::DockBuilderFinish(dockspaceId);

        firstDraw = false;
    }
}

//Generate tree of menu items used to place gui panels in the main menu bar
void MainGui::GenerateMenus()
{
    for (auto& panel : panels_)
    {
        //If empty then the panel is always open and doesn't have a menu entry
        if (panel->MenuPos == "")
        {
            panel->Open = true;
            continue;
        }

        //Split menu path into components
        std::vector<std::string_view> menuParts = String::SplitString(panel->MenuPos, "/");
        string menuName = string(menuParts[0]);

        //Get or create menu
        MenuItem* curMenuItem = GetMenu(menuName);
        if (!curMenuItem)
        {
            menuItems_.push_back(MenuItem{ menuName, {} });
            curMenuItem = &menuItems_.back();
        }

        //Recursively iterate path segments to create menu tree
        for (int i = 1; i < menuParts.size(); i++)
        {
            string nextPart = string(menuParts[i]);
            MenuItem* nextItem = curMenuItem->GetItem(nextPart);
            if (!nextItem)
            {
                curMenuItem->Items.push_back(MenuItem{ nextPart, {} });
                nextItem = &curMenuItem->Items.back();
            }

            curMenuItem = nextItem;
        }

        curMenuItem->panel = panel;
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