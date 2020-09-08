#pragma once
#include "gui/GuiState.h"
#include "render/imgui/ImGuiConfig.h"
#include "imnodes.h"
#include <functional>
#include <spdlog/fmt/fmt.h>
#include "Log.h"

static int current_id = 0;

//Todo: Add type checking for input/output attributes
struct IAttribute
{
    i32 Id;
    virtual void Draw() = 0;
};
struct Node
{
    int id;
    string title;
    std::vector<IAttribute*> Attributes = {};
    ImVec4 TitlebarColor;
    bool UseCustomTitlebarColor = false;

    void AddAttribute(IAttribute* attribute)
    {
        attribute->Id = current_id++;
        Attributes.push_back(attribute);
    }
    Node& SetCustomTitlebarColor(const ImVec4& color)
    {
        UseCustomTitlebarColor = true;
        TitlebarColor = color;
        return *this;
    }
    void Draw()
    {
        if (UseCustomTitlebarColor)
        {
            imnodes::PushColorStyle(
                imnodes::ColorStyle_TitleBar, IM_COL32(TitlebarColor.x, TitlebarColor.y, TitlebarColor.z, TitlebarColor.w));
            //imnodes::PushColorStyle(
            //    imnodes::ColorStyle_TitleBarSelected, IM_COL32(TitlebarColor.x + 10, TitlebarColor.y + 10, TitlebarColor.z + 10, TitlebarColor.w));
        }

        imnodes::BeginNode(id);

        imnodes::BeginNodeTitleBar();
        ImGui::TextUnformatted(title.c_str());
        imnodes::EndNodeTitleBar();

        for (auto& attribute : Attributes)
            attribute->Draw();

        imnodes::EndNode();
        if (UseCustomTitlebarColor)
        {
            imnodes::PopColorStyle();
            //imnodes::PopColorStyle();
        }
    }
    i32 GetAttributeId(u32 index)
    {
        if (index >= Attributes.size())
            THROW_EXCEPTION("Invalid index passed to GetAttributeId");

        return Attributes[index]->Id;
    }
};
struct Link
{
    int id;
    int start_attr, end_attr;
};

//Todo: Free nodes on reload/exit
//Note: Using Node* here so we can easily have stable refs/ptrs to nodes
static std::vector<Node*> Nodes;
static std::vector<Link> Links;

Node& AddNode(const string& title, const std::vector<IAttribute*> attributes)
{
    Nodes.push_back(new Node);
    Node& node = *Nodes.back();
    node.id = current_id++;
    node.title = title;

    for (IAttribute* attribute : attributes)
        node.AddAttribute(attribute);

    return node;
}
void AddLink(i32 start, i32 end)
{
    Links.push_back({ current_id++, start, end });
}

bool IsAttributeLinked(int id)
{
    for (auto& link : Links)
    {
        if (link.start_attr == id || link.end_attr == id)
            return true;
    }

    return false;
}



//Static attributes
struct StaticAttribute_String : IAttribute
{
    string Label;
    string Value;

    StaticAttribute_String(const string& value) : Label("String"), Value(value) {}
    StaticAttribute_String(const string& value, const string& label) : Label(label), Value(value) {}
    void Draw() override
    {
        imnodes::BeginStaticAttribute(Id);
        ImGui::SetNextItemWidth(200.0f);
        ImGui::InputText(Label.c_str(), &Value);
        imnodes::EndStaticAttribute();
    }
};

//Input attributes
struct InputAttribute_Bool : IAttribute
{
    bool OverrideValue = false;

    InputAttribute_Bool(){}
    InputAttribute_Bool(bool override) : OverrideValue(override) {}
    void Draw() override
    {
        imnodes::BeginInputAttribute(Id, imnodes::PinShape::PinShape_CircleFilled);
        ImGui::Text("Condition");
        if (!IsAttributeLinked(Id))
        {
            ImGui::SameLine();
            ImGui::Checkbox(fmt::format("##InputAttribute_Bool{}", Id).c_str(), &OverrideValue);
        }
        imnodes::EndInputAttribute();
    }
};
struct InputAttribute_Number : IAttribute
{
    string Label;
    f32 Value;

    InputAttribute_Number(f32 value) : Label("Number"), Value(value) {}
    InputAttribute_Number(f32 value, const string& label) : Label(label), Value(value) {}
    void Draw() override
    {
        imnodes::BeginInputAttribute(Id, imnodes::PinShape::PinShape_CircleFilled);
        ImGui::Text(Label);
        if (!IsAttributeLinked(Id))
        {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80.0f);
            ImGui::InputFloat(fmt::format("##InputAttribute_Number{}", Id).c_str(), &Value);
        }
        imnodes::EndInputAttribute();
    }
};
struct InputAttribute_String : IAttribute
{
    void Draw() override
    {
        imnodes::BeginInputAttribute(Id, imnodes::PinShape::PinShape_CircleFilled);
        ImGui::Text("String");
        imnodes::EndInputAttribute();
    }
};
struct InputAttribute_Handle : IAttribute
{
    u32 ObjectClass = 0;
    u32 ObjectNumber = 0;

    InputAttribute_Handle(){ }
    InputAttribute_Handle(u32 classnameHash, u32 num) : ObjectClass(classnameHash), ObjectNumber(num) {}
    void Draw() override
    {
        imnodes::BeginInputAttribute(Id, imnodes::PinShape::PinShape_CircleFilled);
        ImGui::Text("Handle");
        if (!IsAttributeLinked(Id))
        {
            //Todo: Automatically set these in the background based on object selected from zone object lists
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80.0f);
            ImGui::InputScalar(fmt::format("Class##InputAttribute_Handle{}", Id).c_str(), ImGuiDataType_U32, &ObjectClass);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80.0f);
            ImGui::InputScalar(fmt::format("Num##InputAttribute_Handle{}", Id).c_str(), ImGuiDataType_U32, &ObjectNumber);
        }
        imnodes::EndInputAttribute();
    }
};
struct InputAttribute_Run : IAttribute
{
    string Label;

    InputAttribute_Run(const string& label = "Run") : Label(label) {}
    void Draw() override
    {
        imnodes::BeginInputAttribute(Id, imnodes::PinShape::PinShape_TriangleFilled);
        ImGui::Text(Label.c_str());
        imnodes::EndInputAttribute();
    }
};

//Output attributes
struct OutputAttribute_Bool : IAttribute
{
    bool Value;
    void Draw() override
    {
        imnodes::BeginOutputAttribute(Id, imnodes::PinShape::PinShape_CircleFilled);
        static bool flag = true;
        ImGui::Checkbox("Value", &flag);
        imnodes::EndOutputAttribute();
    }
};
struct OutputAttribute_Handle : IAttribute
{
    void Draw() override
    {
        imnodes::BeginOutputAttribute(Id, imnodes::PinShape::PinShape_CircleFilled);
        ImGui::Text("Handle");
        imnodes::EndOutputAttribute();
    }
};
struct OutputAttribute_Run : IAttribute
{
    string Label;

    OutputAttribute_Run(const string& label = "Run") : Label(label) {}
    void Draw() override
    {
        imnodes::BeginOutputAttribute(Id, imnodes::PinShape::PinShape_TriangleFilled);
        ImGui::Text(Label.c_str());
        imnodes::EndOutputAttribute();
    }
};

#define BOOL_NODE() AddNode("Bool", { new OutputAttribute_Bool }, {}, {});
#define SCRIPT_NODE(ScriptName) AddNode("Script", { new StaticAttribute_String(ScriptName) }, { new InputAttribute_Bool }, { new OutputAttribute_Run });

void ScriptxEditor_Update(GuiState* state)
{
    if (!ImGui::Begin("Scriptx editor"))
    {
        ImGui::End();
        return;
    }

    state->FontManager->FontL.Push();
    ImGui::Text(ICON_FA_PROJECT_DIAGRAM "Scriptx editor");
    state->FontManager->FontL.Pop();
    ImGui::Separator();

    static bool firstCall = true;
    if (firstCall)
    {
        imnodes::Initialize();
        //Todo: Clear imnodes state and free attributes when finished with them
        firstCall = false;

        const ImVec4 DelayColor(130, 129, 27, 255); //Yellow
        const ImVec4 PrimitiveColor(47, 141, 125, 255); //Light green
        const ImVec4 ScriptColor(255, 129, 27, 255); //Orange
        const ImVec4 ActionColor(33, 106, 183, 255); //Blue
        const ImVec4 FunctionColor(181, 34, 75, 255); //Red

        //Add nodes
        auto& scriptCondition = AddNode("Bool",
            { new OutputAttribute_Bool }
        ).SetCustomTitlebarColor(PrimitiveColor);
        auto& scriptRoot = AddNode("Script",
            { new StaticAttribute_String("Mission_Start", "Name"), new InputAttribute_Bool, new OutputAttribute_Run }
        ).SetCustomTitlebarColor(ScriptColor);
        AddLink(scriptCondition.GetAttributeId(0), scriptRoot.GetAttributeId(1));


        //AddNode("Bool",
        //    { new OutputAttribute_Bool }
        //).SetCustomTitlebarColor(PrimitiveColor);
        auto& node0 = AddNode("hide_hud",
            { new InputAttribute_Bool(true), new InputAttribute_Run }
        ).SetCustomTitlebarColor(ActionColor);
        AddLink(scriptRoot.GetAttributeId(2), node0.GetAttributeId(1));


        auto& node1 = AddNode("player_disable_input",
            { new InputAttribute_Run }
        ).SetCustomTitlebarColor(ActionColor);
        AddLink(scriptRoot.GetAttributeId(2), node1.GetAttributeId(0));


        AddNode("get_player_handle",
            { new OutputAttribute_Handle }
        ).SetCustomTitlebarColor(FunctionColor);
        //AddNode("Bool",
        //    { new OutputAttribute_Bool }
        //).SetCustomTitlebarColor(PrimitiveColor);
        auto& node2 = AddNode("play_animation",
            //Todo: Have string be a dropdown with all valid animations instead on this action
            { new InputAttribute_Handle, new StaticAttribute_String("stand talk short", "anim_action"), new InputAttribute_Bool(true),
              new InputAttribute_Run }
        ).SetCustomTitlebarColor(ActionColor);
        AddLink(scriptRoot.GetAttributeId(2), node2.GetAttributeId(3));


        //AddNode("Bool",
        //    { new OutputAttribute_Bool }
        //).SetCustomTitlebarColor(PrimitiveColor);
        auto& node3 = AddNode("npc_set_ambient_vehicles_disabled",
            { new InputAttribute_Bool(true), new InputAttribute_Run }
        ).SetCustomTitlebarColor(ActionColor);
        AddLink(scriptRoot.GetAttributeId(2), node3.GetAttributeId(1));


        //AddNode("Bool",
        //    { new OutputAttribute_Bool }
        //).SetCustomTitlebarColor(PrimitiveColor);
        auto& node4 = AddNode("player_set_mission_never_die",
            { new InputAttribute_Bool(true), new InputAttribute_Run }
        ).SetCustomTitlebarColor(ActionColor);
        AddLink(scriptRoot.GetAttributeId(2), node4.GetAttributeId(1));


        auto& node5 = AddNode("set_time_of_day",
            { new InputAttribute_Number(9), new InputAttribute_Number(0), new InputAttribute_Run }
        ).SetCustomTitlebarColor(ActionColor);
        AddLink(scriptRoot.GetAttributeId(2), node5.GetAttributeId(2));


        auto& node6 = AddNode("pause_time_of_day",
            { new InputAttribute_Bool(true), new InputAttribute_Run }
        ).SetCustomTitlebarColor(ActionColor);
        AddLink(scriptRoot.GetAttributeId(2), node6.GetAttributeId(1));


        auto& node7 = AddNode("reinforcements_disable",
            { new InputAttribute_Run }
        ).SetCustomTitlebarColor(ActionColor);
        AddLink(scriptRoot.GetAttributeId(2), node7.GetAttributeId(0));


        auto& node8 = AddNode("music_set_limited_level",
            { new StaticAttribute_String("_Melodic_Ambience", "music_threshold"), new InputAttribute_Run }
        ).SetCustomTitlebarColor(ActionColor);
        AddLink(scriptRoot.GetAttributeId(2), node8.GetAttributeId(1));


        //Todo: Make object handle reference zone data
        auto& node9 = AddNode("patrol_pause",
            { new InputAttribute_Handle(0x3C80004E, 140), new InputAttribute_Run }
        ).SetCustomTitlebarColor(ActionColor);
        AddLink(scriptRoot.GetAttributeId(2), node9.GetAttributeId(1));


        auto& node10 = AddNode("delay",
            { new InputAttribute_Number(3, "min"), new InputAttribute_Number(3, "max"), new InputAttribute_Run("Start"), new OutputAttribute_Run("Continue") }
        ).SetCustomTitlebarColor(DelayColor);
        AddNode("spawn_squad",
            { new InputAttribute_Handle(0x3C80001F, 39), new InputAttribute_Run }
        ).SetCustomTitlebarColor(ActionColor);
        AddLink(scriptRoot.GetAttributeId(2), node10.GetAttributeId(1));
    }
    //Todo: Destroy imnodes context later

    imnodes::BeginNodeEditor();

    //Draw nodes
    static bool firstDraw = true;
    static ImVec2 gridPos = { 25.0f, 25.0f };
    for (auto& node : Nodes)
    {
        node->Draw();
        if (firstDraw)
        {
            imnodes::SetNodeGridSpacePos(node->id, gridPos);
            gridPos.x += imnodes::GetNodeDimensions(node->id).x * 1.2f;
            if (gridPos.x > 1200.0f)
            {
                gridPos.x = 25.0f;
                gridPos.y += imnodes::GetNodeDimensions(node->id).y * 1.5f;
            }
        }
    }
    //Draw links
    for (const Link& link : Links)
    {
        imnodes::Link(link.id, link.start_attr, link.end_attr);
    }
    firstDraw = false;
    imnodes::EndNodeEditor();

    //Add created links to Links
    {
        Link link;
        if (imnodes::IsLinkCreated(&link.start_attr, &link.end_attr))
        {
            link.id = ++current_id;
            Links.push_back(link);
        }
    }
    //Remove destroyed links from Links
    {
        int link_id;
        if (imnodes::IsLinkDestroyed(&link_id))
        {
            auto iter = std::find_if(
                Links.begin(), Links.end(), [link_id](const Link& link) -> bool {
                    return link.id == link_id;
                });
            assert(iter != Links.end());
            Links.erase(iter);
        }
    }

    ImGui::End();
}