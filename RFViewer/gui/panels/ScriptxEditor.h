#pragma once
#include "gui/GuiState.h"
#include <types/Vec2.h>
#include <types/Vec4.h>
#include "render/imgui/ImGuiConfig.h"
#include "imnodes.h"
#include <functional>
#include <spdlog/fmt/fmt.h>
#include "Log.h"

static int current_id = 0;

//Todo: Add type checking for input/output attributes
struct Node;
struct IAttribute
{
    i32 Id;
    Node* Parent;
    virtual void Draw() = 0;
};
struct Link
{
    i32 Id; //Link uid
    i32 Start; //Start attribute uid
    i32 End; //End attribute uid
    Node* StartNode; //Node that owns the start attribute
    Node* EndNode; //Node that owns the end attribute
};
struct Node
{
    i32 Id;
    string Title;
    Vec4 TitlebarColor;
    std::vector<IAttribute*> Attributes = {};
    bool UseCustomTitlebarColor = false;

    //Depth in node hierarchy. Used for auto-positioning of nodes
    u32 Depth = 0;
    std::vector<Node*> Subnodes = { };

    //Used by auto-layout system
    bool InitialPosBasedOnParent = false;

    void AddSubnode(Node* subnode)
    {
        Subnodes.push_back(subnode);
    }
    //Set depth + recursively set depth of subnodes
    void SetDepth(u32 newDepth)
    {
        Depth = newDepth;
        UpdateSubnodeDepths();
    }
    //Recursively update depth of subnodes
    void UpdateSubnodeDepths()
    {
        for (auto& subnode : Subnodes)
            subnode->SetDepth(Depth + 1);
    }
    void AddAttribute(IAttribute* attribute)
    {
        attribute->Id = current_id++;
        attribute->Parent = this;
        Attributes.push_back(attribute);
    }
    Node& SetCustomTitlebarColor(const Vec4& color)
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

        imnodes::BeginNode(Id);

        imnodes::BeginNodeTitleBar();
        ImGui::TextUnformatted(Title.c_str());
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
    IAttribute* GetAttribute(u32 index)
    {
        if (index >= Attributes.size())
            return nullptr;

        return Attributes[index];
    }
    void SetPosition(const Vec2& newPos)
    {
        imnodes::SetNodeGridSpacePos(Id, ImVec2(newPos.x, newPos.y));
    }
    void SetPosition(const ImVec2& newPos)
    {
        imnodes::SetNodeGridSpacePos(Id, newPos);
    }

private:
};

//Todo: Free nodes on reload/exit
//Note: Using Node* here so we can easily have stable refs/ptrs to nodes
static std::vector<Node*> Nodes;
static std::vector<Link> Links;

Node& AddNode(const string& title, const std::vector<IAttribute*> attributes)
{
    Nodes.push_back(new Node);
    Node& node = *Nodes.back();
    node.Id = current_id++;
    node.Title = title;

    for (IAttribute* attribute : attributes)
        node.AddAttribute(attribute);

    return node;
}
void AddLink(IAttribute* start, IAttribute* end)
{
    Links.push_back({ current_id++, start->Id, end->Id, start->Parent, end->Parent });
    start->Parent->AddSubnode(end->Parent);
    start->Parent->UpdateSubnodeDepths();
}
bool IsAttributeLinked(int id)
{
    for (auto& link : Links)
    {
        if (link.Start == id || link.End == id)
            return true;
    }

    return false;
}
Node* FindAttributeOwner(i32 attributeId)
{
    for (auto* node : Nodes)
    {
        for (auto* attribute : node->Attributes)
        {
            if (attribute->Id == attributeId)
                return node;
        }
    }

    return nullptr;
}
u32 GetHighestNodeDepth()
{
    u32 max = 0;
    for (auto* node : Nodes)
    {
        if (node->Depth > max)
            max = node->Depth;
    }
    return max;
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

        const Vec4 DelayColor { 130, 129, 27, 255 }; //Yellow
        const Vec4 PrimitiveColor {47, 141, 125, 255}; //Light green
        const Vec4 ScriptColor { 255, 129, 27, 255 }; //Orange
        const Vec4 ActionColor { 33, 106, 183, 255 }; //Blue
        const Vec4 FunctionColor {181, 34, 75, 255}; //Red

        //Add nodes
        auto& scriptCondition = AddNode("Bool",
            { new OutputAttribute_Bool }
        ).SetCustomTitlebarColor(PrimitiveColor);
        auto& scriptRoot = AddNode("Script",
            { new StaticAttribute_String("Mission_Start", "Name"), new InputAttribute_Bool, new OutputAttribute_Run }
        ).SetCustomTitlebarColor(ScriptColor);
        AddLink(scriptCondition.GetAttribute(0), scriptRoot.GetAttribute(1));


        //AddNode("Bool",
        //    { new OutputAttribute_Bool }
        //).SetCustomTitlebarColor(PrimitiveColor);
        auto& node0 = AddNode("hide_hud",
            { new InputAttribute_Bool(true), new InputAttribute_Run }
        ).SetCustomTitlebarColor(ActionColor);
        AddLink(scriptRoot.GetAttribute(2), node0.GetAttribute(1));


        auto& node1 = AddNode("player_disable_input",
            { new InputAttribute_Run }
        ).SetCustomTitlebarColor(ActionColor);
        AddLink(scriptRoot.GetAttribute(2), node1.GetAttribute(0));


        auto& getPlayerHandle = AddNode("get_player_handle",
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
        AddLink(scriptRoot.GetAttribute(2), node2.GetAttribute(3));
        AddLink(getPlayerHandle.GetAttribute(0), node2.GetAttribute(0));

        //AddNode("Bool",
        //    { new OutputAttribute_Bool }
        //).SetCustomTitlebarColor(PrimitiveColor);
        auto& node3 = AddNode("npc_set_ambient_vehicles_disabled",
            { new InputAttribute_Bool(true), new InputAttribute_Run }
        ).SetCustomTitlebarColor(ActionColor);
        AddLink(scriptRoot.GetAttribute(2), node3.GetAttribute(1));


        //AddNode("Bool",
        //    { new OutputAttribute_Bool }
        //).SetCustomTitlebarColor(PrimitiveColor);
        auto& node4 = AddNode("player_set_mission_never_die",
            { new InputAttribute_Bool(true), new InputAttribute_Run }
        ).SetCustomTitlebarColor(ActionColor);
        AddLink(scriptRoot.GetAttribute(2), node4.GetAttribute(1));


        auto& node5 = AddNode("set_time_of_day",
            { new InputAttribute_Number(9), new InputAttribute_Number(0), new InputAttribute_Run }
        ).SetCustomTitlebarColor(ActionColor);
        AddLink(scriptRoot.GetAttribute(2), node5.GetAttribute(2));


        auto& node6 = AddNode("pause_time_of_day",
            { new InputAttribute_Bool(true), new InputAttribute_Run }
        ).SetCustomTitlebarColor(ActionColor);
        AddLink(scriptRoot.GetAttribute(2), node6.GetAttribute(1));


        auto& node7 = AddNode("reinforcements_disable",
            { new InputAttribute_Run }
        ).SetCustomTitlebarColor(ActionColor);
        AddLink(scriptRoot.GetAttribute(2), node7.GetAttribute(0));


        auto& node8 = AddNode("music_set_limited_level",
            { new StaticAttribute_String("_Melodic_Ambience", "music_threshold"), new InputAttribute_Run }
        ).SetCustomTitlebarColor(ActionColor);
        AddLink(scriptRoot.GetAttribute(2), node8.GetAttribute(1));


        //Todo: Make object handle reference zone data
        auto& node9 = AddNode("patrol_pause",
            { new InputAttribute_Handle(0x3C80004E, 140), new InputAttribute_Run }
        ).SetCustomTitlebarColor(ActionColor);
        AddLink(scriptRoot.GetAttribute(2), node9.GetAttribute(1));


        auto& node10 = AddNode("delay",
            { new InputAttribute_Number(3, "min"), new InputAttribute_Number(3, "max"), new InputAttribute_Run("Start"), new OutputAttribute_Run("Continue") }
        ).SetCustomTitlebarColor(DelayColor);
        auto& spawn_squad = AddNode("spawn_squad",
            { new InputAttribute_Handle(0x3C80001F, 39), new InputAttribute_Run }
        ).SetCustomTitlebarColor(ActionColor);
        AddLink(scriptRoot.GetAttribute(2), node10.GetAttribute(1));
        AddLink(node10.GetAttribute(3), spawn_squad.GetAttribute(1));
    }
    //Todo: Destroy imnodes context later

    static bool firstDraw = true;
    static ImVec2 gridPos = { 25.0f, 25.0f };
    
    imnodes::BeginNodeEditor();

    //Draw nodes
    for (auto& node : Nodes)
    {
        node->Draw();
        if (firstDraw)
        {
            u32 maxDepth = GetHighestNodeDepth();
            ImVec2 curPos = { 25.0f, 25.0f };
            for (u32 depth = 0; depth <= maxDepth; depth++)
            {
                f32 widestNodeFound = 0.0f;
                u32 depthCount = 0;
                for (auto* node : Nodes)
                {
                    if (node->Depth != depth)
                        continue;

                    depthCount++;
                    node->SetPosition(curPos);
                    ImVec2 nodeDimensions = imnodes::GetNodeDimensions(node->Id);
                    curPos.y += nodeDimensions.y * 1.2f;
                    if (nodeDimensions.x > widestNodeFound)
                        widestNodeFound = nodeDimensions.x;
                }
                curPos.x += widestNodeFound * 1.5f;
                curPos.y = 25.0f;
            }
        }
    }
    //Draw links
    for (const Link& link : Links)
    {
        imnodes::Link(link.Id, link.Start, link.End);
    }
    firstDraw = false;
    imnodes::EndNodeEditor();

    //Add created links to Links
    {
        Link link;
        if (imnodes::IsLinkCreated(&link.Start, &link.End))
        {
            Node* startNode = FindAttributeOwner(link.Start);
            Node* endNode = FindAttributeOwner(link.End);
            if (!startNode)
                THROW_EXCEPTION("Link created for attribute with no owner!");
            if (!endNode)
                THROW_EXCEPTION("Link created for attribute with no owner!");
            
            link.Id = ++current_id;
            link.StartNode = startNode;
            link.EndNode = endNode;
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
                    return link.Id == link_id;
                });
            assert(iter != Links.end());
            Links.erase(iter);
        }
    }

    ImGui::End();
}