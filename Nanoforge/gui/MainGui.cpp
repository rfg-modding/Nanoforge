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
#include "rfg/TextureIndex.h"
#include "gui/documents/LocalizationDocument.h"
#include "util/Profiler.h"

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

void MainGui::Init(ImGuiFontManager* fontManager, PackfileVFS* packfileVFS, DX11Renderer* renderer, Project* project, XtblManager* xtblManager, Config* config, Localization* localization, TextureIndex* textureSearchIndex)
{
    TRACE();
    State = GuiState{ fontManager, packfileVFS, renderer, project, xtblManager, config, localization, textureSearchIndex };

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
    PROFILER_FUNCTION();

    //Draw always visible UI elements
    DrawMainMenuBar();
    DrawDockspace();
#ifdef DEVELOPMENT_BUILD
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
        if (document->Open)
        {
            ImGui::SetNextWindowDockID(dockspaceCentralNodeId, ImGuiCond_Appearing);
            ImGui::Begin(document->Title.c_str(), &document->Open, document->UnsavedChanges ? ImGuiWindowFlags_UnsavedDocument : 0);
            if (ImGui::IsWindowFocused())
                currentDocument_ = document;

            document->Update(&State);
            ImGui::End();
            iter++;
        }
        else if (!document->Open && !document->UnsavedChanges)
        {
            if (currentDocument_ == *iter)
                currentDocument_ = nullptr;

            iter = State.Documents.erase(iter);
        }
        else
            iter++;
    }

    //Draw close confirmation for documents with unsaved changes
    u32 numUnsavedDocs = std::ranges::count_if(State.Documents, [](Handle<IDocument> doc) { return !doc->Open && doc->UnsavedChanges; });
    if(numUnsavedDocs != 0)
    {
        auto& docs = State.Documents;
        if (!ImGui::IsPopupOpen("Save?"))
            ImGui::OpenPopup("Save?");
        if (ImGui::BeginPopupModal("Save?", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("Save changes to the following file(s)?");
            float itemHeight = ImGui::GetTextLineHeightWithSpacing();
            if (ImGui::BeginChildFrame(ImGui::GetID("frame"), ImVec2(-FLT_MIN, 6.25f * itemHeight)))
            {
                for (auto& doc : docs)
                    if(!doc->Open && doc->UnsavedChanges)
                        ImGui::Text(doc->Title.c_str());

                ImGui::EndChildFrame();
            }

            ImVec2 buttonSize(ImGui::GetFontSize() * 7.0f, 0.0f);
            if (ImGui::Button("Save", buttonSize))
            {
                for (auto& doc : docs)
                {
                    if (!doc->Open && doc->UnsavedChanges)
                        doc->Save(&State);

                    doc->UnsavedChanges = false;
                }
                State.CurrentProject->Save();
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();
            if (ImGui::Button("Don't save", buttonSize))
            {
                for (auto& doc : docs)
                    if (!doc->Open && doc->UnsavedChanges)
                    {
                        doc->UnsavedChanges = false;
                        doc->ResetOnClose = true;
                    }

                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();
            if (ImGui::Button("Cancel", buttonSize))
            {
                for (auto& doc : docs)
                    if (!doc->Open && doc->UnsavedChanges)
                        doc->Open = true;

                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }

    //Draw mod packaging modal
    if (showModPackagingPopup_)
        DrawModPackagingPopup(&showModPackagingPopup_, &State);
    if (showSettingsWindow_)
        DrawSettingsGui(&showSettingsWindow_, State.Config, State.FontManager);

    //Show new/open/close project dialogs once unsaved changes are handled
    numUnsavedDocs = std::ranges::count_if(State.Documents, [](Handle<IDocument> doc) { return doc->UnsavedChanges; });
    if (showNewProjectWindow_ && numUnsavedDocs == 0)
    {
        bool loadedNewProject = DrawNewProjectWindow(&showNewProjectWindow_, State.CurrentProject, State.Config);
        if (loadedNewProject)
            State.Xtbls->ReloadXtbls();
    }
    if (openProjectRequested_ && numUnsavedDocs == 0)
    {
        bool loadedNewProject = TryOpenProject(State.CurrentProject, State.Config);
        openProjectRequested_ = false;
        if (loadedNewProject)
            State.Xtbls->ReloadXtbls();
    }
    if (closeProjectRequested_ && numUnsavedDocs == 0)
    {
        State.CurrentProject->Close();
        closeProjectRequested_ = false;
        State.Xtbls->ReloadXtbls();
    }
    if (openRecentProjectRequested_ && numUnsavedDocs == 0)
    {
        if (std::filesystem::exists(openRecentProjectRequestData_))
        {
            State.CurrentProject->Save();
            State.CurrentProject->Load(openRecentProjectRequestData_);
            State.Xtbls->ReloadXtbls();
        }
        openRecentProjectRequested_ = false;
    }

    //Draw texture search index generation modal
    if (showTextureSearchGenPopup_)
    {
        const char* popupName = "Generating texture search index";
        ImGui::OpenPopup(popupName);
        if (ImGui::BeginPopupModal(popupName))
        {
            ImGui::Text(State.TextureSearchIndex->TextureIndexGenTaskStatus.c_str());
            ImGui::ProgressBar(State.TextureSearchIndex->TextureIndexGenTaskProgressFraction);

            if (State.TextureSearchIndex->TextureIndexGenTask->Completed())
            {
                if (ImGui::Button("Close"))
                {
                    showTextureSearchGenPopup_ = false;
                    ImGui::CloseCurrentPopup();
                }
            }

            ImGui::EndPopup();
        }
    }
}

void MainGui::SaveFocusedDocument()
{
    if (!State.CurrentProject->Loaded() || !currentDocument_ || !currentDocument_->UnsavedChanges)
        return;

    currentDocument_->Save(&State);
    currentDocument_->UnsavedChanges = false;
    State.CurrentProject->Save();
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

                //Close all documents so save confirmation modal appears for them
                for (auto& doc : State.Documents)
                    doc->Open = false;
            }
            if (ImGui::MenuItem("Open project..."))
            {
                //Close all documents so save confirmation modal appears for them
                for (auto& doc : State.Documents)
                    doc->Open = false;

                openProjectRequested_ = true;
            }
            if (ImGui::MenuItem("Save project", nullptr, false, State.CurrentProject->Loaded()))
            {
                State.CurrentProject->Save();
            }
            if (ImGui::MenuItem("Close project", nullptr, false, State.CurrentProject->Loaded()))
            {
                //Close all documents so save confirmation modal appears for them
                for (auto& doc : State.Documents)
                    doc->Open = false;

                closeProjectRequested_ = true;
            }
            if (ImGui::BeginMenu("Recent projects"))
            {
                auto recentProjects = State.Config->GetListReadonly("Recent projects").value();
                for (auto& path : recentProjects)
                {
                    if (ImGui::MenuItem(Path::GetFileName(path).c_str()))
                    {
                        //Close all documents so save confirmation modal appears for them
                        for (auto& doc : State.Documents)
                            doc->Open = false;

                        openRecentProjectRequested_ = true;
                        openRecentProjectRequestData_ = path;
                    }
                }
                ImGui::EndMenu();
            }

            //Save currently focused document if there is one
            bool canSave = currentDocument_ != nullptr && currentDocument_->UnsavedChanges;
            string saveFileName = fmt::format("Save {}", canSave ? currentDocument_->Title : "file");
            if (ImGui::MenuItem(saveFileName.c_str(), "Ctrl + S", nullptr, canSave))
            {
                SaveFocusedDocument();
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
#ifdef DEBUG
            if (ImGui::MenuItem("Generate texture search index") && !showTextureSearchGenPopup_)
            {
                State.TextureSearchIndex->StartTextureIndexGeneration();
                showTextureSearchGenPopup_ = true;
            }
#endif
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
            cursorPos.x += 45.0f;
        }

        //Draw warning if the profiler is enabled so it's not accidentally compiled into release builds.
        //Can cause huge memory usage as it'll buffer the data locally until it connects to the tracy client, which most users won't be running.
#ifdef TRACY_ENABLE
        const char* text = ICON_FA_EXCLAMATION_TRIANGLE " PROFILER ENABLED";
        cursorPos.y += 1.0f;
        drawList->AddText(cursorPos, ImGui::GetColorU32({ 1.0f, 0.5f, 0.153f, 1.0f }), text, text + strlen(text));
        cursorPos.x += ImGui::CalcTextSize(text).x;
        cursorPos.y -= 1.0f;
#endif

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
        dockspaceCentralNodeId = ImGui::DockBuilderGetCentralNode(dockspaceId)->ID;
        ImGuiID dockCentralDownSplitId = ImGui::DockBuilderSplitNode(dockspaceCentralNodeId, ImGuiDir_Down, 0.20f, nullptr, &dockspaceCentralNodeId);

        //Todo: Tie titles to these calls so both copies don't need to be updated every time they change
        ImGui::DockBuilderDockWindow("Start page", dockspaceCentralNodeId);
        ImGui::DockBuilderDockWindow("File explorer", dockLeftId);
        ImGui::DockBuilderDockWindow("Dear ImGui Demo", dockLeftId);
        ImGui::DockBuilderDockWindow("Zones", dockLeftId);
        ImGui::DockBuilderDockWindow("Zone objects", dockLeftBottomId);
        ImGui::DockBuilderDockWindow("Properties", dockRightId);
        ImGui::DockBuilderDockWindow("Render settings", dockRightId);
        ImGui::DockBuilderDockWindow("Scriptx viewer", dockspaceCentralNodeId);
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