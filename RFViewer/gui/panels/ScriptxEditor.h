#pragma once
#include "gui/GuiState.h"
#include <types/Vec2.h>
#include <types/Vec4.h>
#include "render/imgui/ImGuiConfig.h"
#include "imnodes.h"
#include <functional>
#include <spdlog/fmt/fmt.h>
#include "Log.h"
#include <tinyxml2/tinyxml2.h>

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

#pragma region NodeMemberFuncs
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
#pragma endregion NodeMemberFuncs


private:
};

//Todo: Free nodes on reload/exit
//Note: Using Node* here so we can easily have stable refs/ptrs to nodes
static std::vector<Node*> Nodes;
static std::vector<Link> Links;

#pragma region NodeUtilFuncs

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

#pragma endregion NodeUtilFuncs

#pragma region Attributes

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

    InputAttribute_Handle() { }
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

#pragma endregion Attributes

const Vec4 DelayColor{ 130, 129, 27, 255 }; //Yellow
const Vec4 PrimitiveColor{ 47, 141, 125, 255 }; //Light green
const Vec4 ScriptColor{ 255, 129, 27, 255 }; //Orange
const Vec4 ActionColor{ 33, 106, 183, 255 }; //Blue
const Vec4 FunctionColor{ 181, 34, 75, 255 }; //Red

void ScriptxEditor_Cleanup(GuiState* state);

//Load a scriptx file and generate a node graph from it. Clears any existing data/graphs before loading new one
void LoadScriptxFile(const string& name, GuiState* state)
{
    //Clear existing data
    ScriptxEditor_Cleanup(state);

    //Wait for packfileVFS to be ready for use
    //Todo: Either move packfileVFS init out of worker thread or move this onto a thread so this doesn't lock the entire program while packfileVFS init is pending
    while (!state->PackfileVFS->Ready())
    {
        Sleep(100);
    }

    //Find target file
    auto handles = state->PackfileVFS->GetFiles(name, false, true);
    if (handles.size() < 1)
    {
        Log->info("Failed to load scriptx file {}. Unable to locate.", name);
        return;
    }

    //Get scriptx bytes and pass to xml parser
    auto& handle = handles[0];
    std::span<u8> scriptxBytes = handle.Get();
    tinyxml2::XMLDocument* doc = new tinyxml2::XMLDocument;
    doc->Parse((const char*)scriptxBytes.data(), scriptxBytes.size_bytes());

    //Parse scriptx. First get the root element
    auto* root = doc->RootElement();
    if (!root)
        THROW_EXCEPTION("Failed to find root node in scriptx file \"{}\"", name);

    //Todo: Read version, modified, and vars
    //Todo: Support blocks other than <managed> and selecting them
    auto* managed = root->FirstChildElement("managed");
    if (!managed)
        return;

    //Todo: Read all script nodes instead of just the first one
    auto* scriptRootXml = managed->FirstChildElement("script");
    //Parse script node
    {
        //Todo: Support condition node holding actions/functions
        //Get and read condition node
        auto* scriptConditionXml = scriptRootXml->FirstChildElement("condition");
        if (!scriptConditionXml)
            THROW_EXCEPTION("Failed to get <condition> node from scriptx <script> node \"{}\"", scriptRootXml->Value());

        //Note: Currently assumes the condition is just a true/false flag. Need to be more complex to support all scriptx
        auto* scriptCondFlagXml = scriptConditionXml->FirstChildElement("flag");
        if (!scriptCondFlagXml)
            THROW_EXCEPTION("Assumption that script <condition> block only contains a true/false flag failed! Parsing more complex <condition> blocks not yet supported.");

        //Create condition node on node graph
        bool scriptCondFlagValue = string(scriptCondFlagXml->GetText()) == "true" ? true : false;
        auto& scriptCondition = AddNode("Bool",
            { new OutputAttribute_Bool }
        ).SetCustomTitlebarColor(PrimitiveColor);

        //Create script node on graph and link to condition node
        auto& scriptRoot = AddNode("Script",
            { new StaticAttribute_String(scriptRootXml->GetText(), "Name"), new InputAttribute_Bool, new OutputAttribute_Run }
        ).SetCustomTitlebarColor(ScriptColor);
        AddLink(scriptCondition.GetAttribute(0), scriptRoot.GetAttribute(1));

        //Read <script> contents and generate node graph from them
        tinyxml2::XMLElement* currentNode = scriptConditionXml;
        currentNode = currentNode->NextSiblingElement();
        while (currentNode)
        {
            //Todo: Will want to split these into multiple recursive parsing functions since actions can contain actions recursively

            //Parse node
            string nodeTypeName(currentNode->Value());
            if (nodeTypeName == "action")
            {
                Log->info("Reached <action> node");
            }
            else if (nodeTypeName == "variable")
            {
                Log->info("Reached <variable> node");
            }
            else if (nodeTypeName == "delay")
            {
                Log->info("Reached <delay> node");
            }
            else
            {
                Log->warn("Unknown script node type \"{}\" reached. Skipping.", currentNode->Value());
            }


            //Get next node
            currentNode = currentNode->NextSiblingElement();
        }
    }
}

//Clear resources created by scriptx editor & node graph
void ScriptxEditor_Cleanup(GuiState* state)
{
    //Free nodes and attributes. They're allocated on the heap so they're pointers are stable
    for (Node* node : Nodes)
    {
        for (IAttribute* attribute : node->Attributes)
        {
            delete attribute;
        }
        delete node;
    }

    //Clear node and link vectors
    Nodes.clear();
    Links.clear();
}

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

        LoadScriptxFile("terr01_tutorial.scriptx", state);

        ////Add nodes
        //auto& scriptCondition = AddNode("Bool",
        //    { new OutputAttribute_Bool }
        //).SetCustomTitlebarColor(PrimitiveColor);
        //auto& scriptRoot = AddNode("Script",
        //    { new StaticAttribute_String("Mission_Start", "Name"), new InputAttribute_Bool, new OutputAttribute_Run }
        //).SetCustomTitlebarColor(ScriptColor);
        //AddLink(scriptCondition.GetAttribute(0), scriptRoot.GetAttribute(1));


        ////AddNode("Bool",
        ////    { new OutputAttribute_Bool }
        ////).SetCustomTitlebarColor(PrimitiveColor);
        //auto& node0 = AddNode("hide_hud",
        //    { new InputAttribute_Bool(true), new InputAttribute_Run }
        //).SetCustomTitlebarColor(ActionColor);
        //AddLink(scriptRoot.GetAttribute(2), node0.GetAttribute(1));


        //auto& node1 = AddNode("player_disable_input",
        //    { new InputAttribute_Run }
        //).SetCustomTitlebarColor(ActionColor);
        //AddLink(scriptRoot.GetAttribute(2), node1.GetAttribute(0));


        //auto& getPlayerHandle = AddNode("get_player_handle",
        //    { new OutputAttribute_Handle }
        //).SetCustomTitlebarColor(FunctionColor);
        ////AddNode("Bool",
        ////    { new OutputAttribute_Bool }
        ////).SetCustomTitlebarColor(PrimitiveColor);
        //auto& node2 = AddNode("play_animation",
        //    //Todo: Have string be a dropdown with all valid animations instead on this action
        //    { new InputAttribute_Handle, new StaticAttribute_String("stand talk short", "anim_action"), new InputAttribute_Bool(true),
        //      new InputAttribute_Run }
        //).SetCustomTitlebarColor(ActionColor);
        //AddLink(scriptRoot.GetAttribute(2), node2.GetAttribute(3));
        //AddLink(getPlayerHandle.GetAttribute(0), node2.GetAttribute(0));

        ////AddNode("Bool",
        ////    { new OutputAttribute_Bool }
        ////).SetCustomTitlebarColor(PrimitiveColor);
        //auto& node3 = AddNode("npc_set_ambient_vehicles_disabled",
        //    { new InputAttribute_Bool(true), new InputAttribute_Run }
        //).SetCustomTitlebarColor(ActionColor);
        //AddLink(scriptRoot.GetAttribute(2), node3.GetAttribute(1));


        ////AddNode("Bool",
        ////    { new OutputAttribute_Bool }
        ////).SetCustomTitlebarColor(PrimitiveColor);
        //auto& node4 = AddNode("player_set_mission_never_die",
        //    { new InputAttribute_Bool(true), new InputAttribute_Run }
        //).SetCustomTitlebarColor(ActionColor);
        //AddLink(scriptRoot.GetAttribute(2), node4.GetAttribute(1));


        //auto& node5 = AddNode("set_time_of_day",
        //    { new InputAttribute_Number(9), new InputAttribute_Number(0), new InputAttribute_Run }
        //).SetCustomTitlebarColor(ActionColor);
        //AddLink(scriptRoot.GetAttribute(2), node5.GetAttribute(2));


        //auto& node6 = AddNode("pause_time_of_day",
        //    { new InputAttribute_Bool(true), new InputAttribute_Run }
        //).SetCustomTitlebarColor(ActionColor);
        //AddLink(scriptRoot.GetAttribute(2), node6.GetAttribute(1));


        //auto& node7 = AddNode("reinforcements_disable",
        //    { new InputAttribute_Run }
        //).SetCustomTitlebarColor(ActionColor);
        //AddLink(scriptRoot.GetAttribute(2), node7.GetAttribute(0));


        //auto& node8 = AddNode("music_set_limited_level",
        //    { new StaticAttribute_String("_Melodic_Ambience", "music_threshold"), new InputAttribute_Run }
        //).SetCustomTitlebarColor(ActionColor);
        //AddLink(scriptRoot.GetAttribute(2), node8.GetAttribute(1));


        ////Todo: Make object handle reference zone data
        //auto& node9 = AddNode("patrol_pause",
        //    { new InputAttribute_Handle(0x3C80004E, 140), new InputAttribute_Run }
        //).SetCustomTitlebarColor(ActionColor);
        //AddLink(scriptRoot.GetAttribute(2), node9.GetAttribute(1));


        //auto& node10 = AddNode("delay",
        //    { new InputAttribute_Number(3, "min"), new InputAttribute_Number(3, "max"), new InputAttribute_Run("Start"), new OutputAttribute_Run("Continue") }
        //).SetCustomTitlebarColor(DelayColor);
        //auto& spawn_squad = AddNode("spawn_squad",
        //    { new InputAttribute_Handle(0x3C80001F, 39), new InputAttribute_Run }
        //).SetCustomTitlebarColor(ActionColor);
        //AddLink(scriptRoot.GetAttribute(2), node10.GetAttribute(1));
        //AddLink(node10.GetAttribute(3), spawn_squad.GetAttribute(1));
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