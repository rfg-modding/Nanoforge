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
#include "gui/panels/ZoneRender.h"
#include "Log.h"
#include <imgui/imgui.h>
#include <imgui_internal.h>


MainGui::~MainGui()
{
    node::DestroyEditor(State.NodeEditor);
}

void MainGui::Init(ImGuiFontManager* fontManager, PackfileVFS* packfileVFS, Camera* camera, ZoneManager* zoneManager)
{
    State = GuiState{ fontManager, packfileVFS, camera, zoneManager };

    //Create node editor library context
    State.NodeEditor = node::CreateEditor();

    //Register all gui panels
    panels_ =
    {
        GuiPanel{&FileExplorer_Update},
        GuiPanel{&CameraPanel_Update},
        GuiPanel{&RenderSettings_Update},
        GuiPanel{&StatusBar_Update},
        GuiPanel{&ZoneObjectsList_Update},
        GuiPanel{&ZoneRender_Update},

        //Todo: Enable in release builds when this is a working feature
#ifdef DEBUG_BUILD
        GuiPanel{&ScriptxEditor_Update},
#endif
        GuiPanel{&ZoneList_Update},
    };
}

void MainGui::Update(f32 deltaTime)
{
    //Draw built in / special gui elements
    DrawMainMenuBar();
    DrawDockspace();
    ImGui::ShowDemoWindow();

    //Draw the rest of the gui code
    for (auto& panel : panels_)
    {
#ifdef DEBUG_BUILD
        //Todo: Add panel name to the error
        if (!panel.Update)
            THROW_EXCEPTION("Error! Update function pointer for panel was nullptr.");
#endif

        panel.Update(&State);
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
        )
        ImGuiMenu("Help",
            ImGuiMenuItemShort("Welcome", )
            ImGuiMenuItemShort("Metrics", )
            ImGuiMenuItemShort("About", )
        )

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