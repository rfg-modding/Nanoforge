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
#include <imgui.h>
#include <imgui_internal.h>
#include <spdlog/fmt/fmt.h>
#include "rfg/TextureIndex.h"
#include "gui/documents/LocalizationDocument.h"
#include "util/Profiler.h"
#include "rfg/xtbl/XtblManager.h"
#include "util/TaskScheduler.h"
#include "common/filesystem/Path.h"

CVar CVar_ShowFPS("Show FPS", ConfigType::Bool,
    "If enabled an FPS meter is shown on the main menu bar.",
    ConfigValue(false), //Default value
    true  //ShowInSettings
);

CVar CVar_UI_Theme("UI Theme", ConfigType::String,
                   "Color scheme & styling used by the user interface.",
                   ConfigValue("dark"), //Default value
                   false //Show in settings
);

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

void MainGui::Init(ImGuiFontManager* fontManager, PackfileVFS* packfileVFS, DX11Renderer* renderer, Project* project, XtblManager* xtblManager, Localization* localization, TextureIndex* textureSearchIndex)
{
    TRACE();
    State = GuiState{ fontManager, packfileVFS, renderer, project, xtblManager, localization, textureSearchIndex };

    //Create all gui panels
    AddMenuItem("", true, CreateHandle<StatusBar>());
    AddMenuItem("View/Start page", true, CreateHandle<StartPanel>());
    AddMenuItem("View/Properties", false, CreateHandle<PropertyPanel>());
    AddMenuItem("View/Log", false, CreateHandle<LogPanel> ());
    AddMenuItem("View/Zone objects", false, CreateHandle<ZoneObjectsList>());
    AddMenuItem("View/Zone list", false, CreateHandle<ZoneList>());
    AddMenuItem("View/File explorer", true, CreateHandle<FileExplorer>());
    AddMenuItem("View/Scriptx viewer (WIP)", false, CreateHandle<ScriptxEditor>(&State));

    GenerateMenus();

    //Set UI theme
    string& themePreset = CVar_UI_Theme.Get<string>();
    if (String::EqualIgnoreCase(themePreset, "dark"))
        gui::SetThemePreset(Dark);
    else if (String::EqualIgnoreCase(themePreset, "orange"))
        gui::SetThemePreset(Orange);
    else if (String::EqualIgnoreCase(themePreset, "blue"))
        gui::SetThemePreset(Blue);
    else
    {
        gui::SetThemePreset(Dark);
        themePreset = "dark";
        Config::Get()->Save();
    }
}

void MainGui::Update()
{
    PROFILER_FUNCTION();

    //Update UI scale if it changed since last frame
    static f32 lastFrameScale = 1.0f;
    if (lastFrameScale != CVar_UIScale.Get<f32>())
    {
        lastFrameScale = CVar_UIScale.Get<f32>();
        ImGui::GetIO().FontGlobalScale = lastFrameScale;
    }

    //Draw always visible UI elements
    DrawMainMenuBar();
    DrawDockspace();
#ifdef DEVELOPMENT_BUILD
    if (imguiDemoWindowOpen_)
    {
        ImGui::ShowDemoWindow(&imguiDemoWindowOpen_);
    }
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
            //Optionally disable window padding for documents that need to flush with the window (map/mesh viewer viewports)
            if (document->NoWindowPadding)
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.0f, 0.0f });

            ImGui::SetNextWindowDockID(dockspaceCentralNodeId, ImGuiCond_Appearing);
            ImGui::Begin(document->Title.c_str(), &document->Open, document->UnsavedChanges ? ImGuiWindowFlags_UnsavedDocument : 0);
            if (ImGui::IsWindowFocused())
                currentDocument_ = document;

            document->Update(&State);
            ImGui::End();

            if (document->NoWindowPadding)
                ImGui::PopStyleVar();

            //Call OnClose when the user clicks the close button
            if (!document->Open)
                document->OnClose(&State);

            iter++;
        }
        else if (!document->Open && !document->UnsavedChanges && document->CanClose())
        {
            //Document was closed by user, has no unsaved changes, and is ready to close (not waiting for threads to finish)
            if (currentDocument_ == *iter)
                currentDocument_ = nullptr;

            iter = State.Documents.erase(iter);
        }
        else
            iter++;
    }

    //Draw close confirmation for documents with unsaved changes
    u32 numUnsavedDocs = (u32)std::ranges::count_if(State.Documents, [](Handle<IDocument> doc) { return !doc->Open && doc->UnsavedChanges; });
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
        DrawSettingsGui(&showSettingsWindow_, State.FontManager);

    //Show new/open/close project dialogs once unsaved changes are handled
    numUnsavedDocs = (u32)std::ranges::count_if(State.Documents, [](Handle<IDocument> doc) { return doc->UnsavedChanges; });
    if (showNewProjectWindow_ && numUnsavedDocs == 0)
    {
        bool loadedNewProject = DrawNewProjectWindow(&showNewProjectWindow_, State.CurrentProject);
        if (loadedNewProject)
            State.Xtbls->ReloadXtbls();
    }
    if (openProjectRequested_ && numUnsavedDocs == 0)
    {
        bool loadedNewProject = TryOpenProject(State.CurrentProject);
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

void MainGui::AddMenuItem(std::string_view menuPos, bool open, Handle<IGuiPanel> panel)
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
    const f32 mainMenuFramePaddingY = 8.0f;
    const f32 mainMenuContentStartY = 5.0f;
    mainMenuHeight = (1.0f * ImGui::GetFont()->FontSize) + (2.0f * mainMenuFramePaddingY);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { ImGui::GetStyle().FramePadding.x, mainMenuFramePaddingY });
    if (ImGui::BeginMainMenuBar())
    {
        ImGui::PopStyleVar();

        //Track mouse movement in screen coords
        static int moveOffsetX = 0;
        static int moveOffsetY = 0;
        HWND hwnd = State.Renderer->GetSystemWindowHandle();
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            POINT point;
            RECT rect;

            GetCursorPos(&point);
            GetWindowRect(hwnd, &rect);

            // Calculate the difference between the cursor pos and window pos
            moveOffsetX = point.x - rect.left;
            moveOffsetY = point.y - rect.top;
        }

        //Move native window if user click-drags the imgui main menu bar
        if ((moveOffsetY >= 0 && moveOffsetY <= (i32)mainMenuHeight) && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) //Cursor must be *on* the titlebar
        {
            POINT point;
            GetCursorPos(&point);

            SetWindowPos(hwnd, nullptr, point.x - moveOffsetX, point.y - moveOffsetY, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        }


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
                std::vector<string>& recentProjects = CVar_RecentProjects.Get<std::vector<string>>();
                for (string& path : recentProjects)
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

        if (ImGui::BeginMenu("View"))
        {
            if (ImGui::BeginMenu("Layout"))
            {
                if (ImGui::MenuItem("Default"))
                {
                    //Todo: Give panels and documents separate identifiers so their titles can change without breaking everything
                    SetPanelVisibility("Start page", true);
                    SetPanelVisibility(ICON_FA_WRENCH " Properties", false);
                    SetPanelVisibility("Log", false);
                    SetPanelVisibility(ICON_FA_BOXES " Zone objects", false);
                    SetPanelVisibility(ICON_FA_MAP " Zones", false);
                    SetPanelVisibility("File explorer", true);
                    SetPanelVisibility("Scriptx viewer", false);
                }
                if (ImGui::MenuItem("Level editing"))
                {
                    SetPanelVisibility("Start page", false);
                    SetPanelVisibility(ICON_FA_WRENCH " Properties", true);
                    SetPanelVisibility("Log", false);
                    SetPanelVisibility(ICON_FA_BOXES " Zone objects", true);
                    SetPanelVisibility(ICON_FA_MAP " Zones", false);
                    SetPanelVisibility("File explorer", true);
                    SetPanelVisibility("Scriptx viewer", false);
                }
                ImGui::EndMenu();
            }
#ifdef DEVELOPMENT_BUILD
            if (ImGui::MenuItem("ImGui Demo", nullptr, &imguiDemoWindowOpen_))
            {

            }
#endif
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
#ifdef DEBUG_BUILD
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
                CVar_UI_Theme.Get<string>() = "dark";
                Config::Get()->Save();
            }
            if (ImGui::MenuItem("Orange"))
            {
                gui::SetThemePreset(Orange);
                CVar_UI_Theme.Get<string>() = "orange";
                Config::Get()->Save();
            }
            if (ImGui::MenuItem("Blue"))
            {
                gui::SetThemePreset(Blue);
                CVar_UI_Theme.Get<string>() = "blue";
                Config::Get()->Save();
            }
            ImGui::EndMenu();
        }

        bool showFPS = CVar_ShowFPS.Get<bool>();
        ImVec2 cursorPos = { ImGui::GetCursorPosX(), mainMenuContentStartY + 2.0f }; // mainMenuFramePaddingY
        auto* drawList = ImGui::GetWindowDrawList();

        //Draw project name
        string projectName = State.CurrentProject->Loaded() ? State.CurrentProject->Name : "No project loaded";
        projectName = "|    " + projectName;
        drawList->AddText(cursorPos, 0xF2F5FAFF, projectName.c_str(), projectName.c_str() + projectName.length());

        //cursorPos.y = mainMenuContentStartY;
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

        //Draw Nanoforge version at the far right
        f32 versionTextWidth = ImGui::CalcTextSize(BuildConfig::Version.c_str()).x;
        f32 padding = 8.0f;
        f32 rightPadding = 166.0f;
        f32 versionTextPadding = 16.0f; //TODO: CLEAN UP THIS CODE/VARIABLES
        cursorPos = { ImGui::GetWindowWidth() - versionTextWidth - padding - rightPadding - versionTextPadding, mainMenuContentStartY };
        drawList->AddText(cursorPos, 0xF2F5FAFF, BuildConfig::Version.c_str(), BuildConfig::Version.c_str() + BuildConfig::Version.length());

        //Draw minimize, maximize, and close buttons
        {
            ImVec2 windowButtonSize = { 58.0f, 40.0f };
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 0.0f, ImGui::GetStyle().ItemSpacing.y });
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetColorU32(ImGuiCol_MenuBarBg));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetColorU32(ImGuiCol_ScrollbarGrabActive));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetColorU32(ImGuiCol_ScrollbarGrabHovered));
            ImGui::SetCursorPosX(ImGui::GetWindowWidth() - padding - rightPadding);
            ImGui::SetCursorPosY(0.0f); //mainMenuContentStartY + 1.0f);// (0.5f * ImGui::GetFont()->FontSize));

            //Minimize button
            if (ImGui::Button(ICON_VS_CHROME_MINIMIZE, windowButtonSize))
            {
                ShowWindow(hwnd, SW_SHOWMINIMIZED);
            }

            //Maximize button. Icon changes depending on if the window is maximized currently
            ImGui::SameLine();
            if (IsZoomed(hwnd))
            {
                if (ImGui::Button(ICON_VS_CHROME_RESTORE, windowButtonSize)) //Window already maximized
                    ShowWindow(hwnd, SW_SHOWDEFAULT);
            }
            else
            {
                if (ImGui::Button(ICON_VS_CHROME_MAXIMIZE, windowButtonSize)) //Window not maximized
                    ShowWindow(hwnd, SW_SHOWMAXIMIZED);
            }

            //Close button
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetColorU32({ 197.0f / 255.0f, 20.0f / 255.0f, 43.0f / 255.0f, 1.0f }));
            if (ImGui::Button(ICON_VS_CHROME_CLOSE, windowButtonSize))
            {
                Shutdown = true;
            }
            ImGui::PopStyleColor();
            ImGui::PopStyleColor(3);
            ImGui::PopStyleVar(3);
        }
        ImGui::EndMainMenuBar();
    }

    //Draw toolbar
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 toolbarPos = viewport->WorkPos;
    ImVec2 toolbarSize = { viewport->WorkSize.x, 32.0f };
    toolbarHeight = toolbarSize.y;
    ImGui::SetNextWindowPos(toolbarPos);
    ImGui::SetNextWindowSize(toolbarSize, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImGui::GetColorU32(ImGuiCol_MenuBarBg));
    if (ImGui::Begin("##Toolbar0", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize))
    {
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetColorU32(ImGuiCol_MenuBarBg));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetColorU32(ImGuiCol_ScrollbarGrabActive));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetColorU32(ImGuiCol_ScrollbarGrabHovered));
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 8.0f);
        if (ImGui::Button(ICON_FA_UNDO))
        {

        }
        gui::TooltipOnPrevious("Undo");
        ImGui::SameLine();
        if (ImGui::Button(ICON_FA_REDO))
        {

        }
        gui::TooltipOnPrevious("Redo");
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();

        ImGui::End();
    }
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
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
    ImVec2 dockspacePos = viewport->WorkPos;
    dockspacePos.y += toolbarHeight;
    ImVec2 dockspaceSize = viewport->WorkSize;
    dockspaceSize.y -= State.StatusBarHeight;
    dockspaceSize.y -= toolbarHeight;
    dockspaceSize.y += 1; //Cover up a 1 pixel wide line separating the dockspace and status bar
    ImGui::SetNextWindowPos(dockspacePos);
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
            ImGuiDockNode* dockspaceNode = ImGui::DockBuilderGetNode(dockspaceId);
            dockspaceNode->LocalFlags |= ImGuiDockNodeFlags_NoWindowMenuButton | ImGuiDockNodeFlags_NoCloseButton; //Disable extra close button on dockspace. Tabs will still have their own.
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
        ImGuiID dockRightId = ImGui::DockBuilderSplitNode(dockspaceId, ImGuiDir_Right, 0.25f, nullptr, &dockspaceId);
        ImGuiID dockRightUp = ImGui::DockBuilderSplitNode(dockRightId, ImGuiDir_Up, 0.3f, nullptr, &dockRightId);
        dockspaceCentralNodeId = ImGui::DockBuilderGetCentralNode(dockspaceId)->ID;
        ImGuiID dockCentralDownSplitId = ImGui::DockBuilderSplitNode(dockspaceCentralNodeId, ImGuiDir_Down, 0.20f, nullptr, &dockspaceCentralNodeId);

        //Todo: Tie titles to these calls so both copies don't need to be updated every time they change
        ImGui::DockBuilderDockWindow("Start page", dockspaceCentralNodeId);
        ImGui::DockBuilderDockWindow("File explorer", dockLeftId);
        ImGui::DockBuilderDockWindow("Dear ImGui Demo", dockLeftId);
        ImGui::DockBuilderDockWindow(ICON_FA_MAP " Zones", dockLeftId);
        ImGui::DockBuilderDockWindow(ICON_FA_BOXES " Zone objects", dockRightUp);
        ImGui::DockBuilderDockWindow(ICON_FA_WRENCH " Properties", dockRightId);
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

MenuItem* MainGui::GetMenu(std::string_view text)
{
    for (auto& item : menuItems_)
    {
        if (item.Text == text)
            return &item;
    }
    return nullptr;
}

void MainGui::SetPanelVisibility(const std::string& title, bool visible)
{
    for (Handle<IGuiPanel> panel : panels_)
    {
        if (panel->Title == title)
        {
            panel->Open = visible;
            return;
        }
    }
}
