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

enum AttributeType
{
    Input,
    Output,
    Static
};

//Todo: Add type checking for input/output attributes
struct Node;
struct IAttribute
{
    i32 Id;
    Node* Parent;
    AttributeType Type = Static;
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
    IAttribute* AddAttribute(IAttribute* attribute)
    {
        attribute->Id = current_id++;
        attribute->Parent = this;
        Attributes.push_back(attribute);
        return Attributes.back();
    }
    Node* SetCustomTitlebarColor(const Vec4& color)
    {
        UseCustomTitlebarColor = true;
        TitlebarColor = color;
        return this;
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

Node* AddNode(const string& title, const std::vector<IAttribute*> attributes)
{
    Nodes.push_back(new Node);
    Node& node = *Nodes.back();
    node.Id = current_id++;
    node.Title = title;

    for (IAttribute* attribute : attributes)
        node.AddAttribute(attribute);

    return &node;
}
void AddLink(IAttribute* start, IAttribute* end)
{
    Links.push_back({ current_id++, start->Id, end->Id, start->Parent, end->Parent });
    start->Parent->AddSubnode(end->Parent);

    //If node already has a depth it creates better looking graphs to just walk starts depth back one from end rather than set it to 0
    if (end->Parent->Depth != 0)
        start->Parent->Depth = end->Parent->Depth - 1;

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

    StaticAttribute_String(const string& value) : Label("String"), Value(value) { Type = Static; }
    StaticAttribute_String(const string& value, const string& label) : Label(label), Value(value) { Type = Static; }
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
    string Label = "Flag";

    InputAttribute_Bool() { Type = Input; }
    InputAttribute_Bool(bool override, const string& label) : OverrideValue(override), Label(label) { Type = Input; }
    void Draw() override
    {
        imnodes::BeginInputAttribute(Id, imnodes::PinShape::PinShape_CircleFilled);
        ImGui::Text(Label);
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

    InputAttribute_Number(f32 value) : Label("Number"), Value(value) { Type = Input; }
    InputAttribute_Number(f32 value, const string& label) : Label(label), Value(value) { Type = Input; }
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
    InputAttribute_String() { Type = Input; }
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

    InputAttribute_Handle() { Type = Input; }
    InputAttribute_Handle(u32 classnameHash, u32 num) : ObjectClass(classnameHash), ObjectNumber(num) { Type = Input; }
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

    InputAttribute_Run(const string& label = "Run") : Label(label) { Type = Input; }
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

    OutputAttribute_Bool() { Type = Output; }
    void Draw() override
    {
        imnodes::BeginOutputAttribute(Id, imnodes::PinShape::PinShape_CircleFilled);
        static bool flag = true;
        ImGui::Checkbox(fmt::format("Value##{}", Id).c_str(), &flag);
        imnodes::EndOutputAttribute();
    }
};
struct OutputAttribute_Handle : IAttribute
{
    OutputAttribute_Handle() { Type = Output; }
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

    OutputAttribute_Run(const string& label = "Run") : Label(label) { Type = Output; }
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
const Vec4 EventColor{ 97, 47, 140, 255 }; //Purple

void ScriptxEditor_Cleanup(GuiState* state);
void ParseScriptxNode(tinyxml2::XMLElement* xmlNode, Node* graphParent);

//std::vector<const char*> ScriptList = {};
string ScriptList = "";
u32 TargetScriptIndex = 0;
void SetTargetScript(u32 newTargetIndex)
{
    //if (newTargetIndex >= ScriptList.size())
    //    return;

    TargetScriptIndex = newTargetIndex;
}

//Todo: Move attributes into their own files + move most scriptx parsing stuff into a class + organize it better. Currently a mess. Will do once it's most working.
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
    //Todo: Free this memory once done with it
    tinyxml2::XMLDocument* doc = new tinyxml2::XMLDocument;
    doc->Parse((const char*)scriptxBytes.data(), scriptxBytes.size_bytes());

    //Parse scriptx. First get the root element
    auto* root = doc->RootElement();
    if (!root)
        THROW_EXCEPTION("Failed to find root node in scriptx file \"{}\"", name);

    //Todo: Read version, modified, and vars
    //Find all <managed> and <group> blocks. Parse <script> blocks within them.
    auto* curGroup = root->FirstChildElement();
    bool generateScriptList = ScriptList == ""; //Only gen script list if it's empty
    u32 index = 0; //Todo: Rename to scriptIndex
    while (curGroup)
    {
        string groupNodeType(curGroup->Value());
        //Skip nodes that aren't <managed> or <group>. All <script> blocks should be inside one of those
        if (groupNodeType != "managed" && groupNodeType != "group")
        {
            curGroup = curGroup->NextSiblingElement();
            continue;
        }

        //Todo: Update script name list in this loop somewhere
        //Parse all scripts inside group
        auto* scriptRootXml = curGroup->FirstChildElement("script");
        while (scriptRootXml)
        {
            //Add script name to script list if we're generating it this run
            if (generateScriptList)
            {
                //Get script name and strip extraneous newlines and tabs
                string scriptName(scriptRootXml->GetText());
                size_t newlinePos = scriptName.find('\n');
                if (newlinePos != string::npos && newlinePos != scriptName.size() - 1) //Clear extra data like tabs after newline
                    scriptName = scriptName.erase(newlinePos);

                ScriptList += scriptName + '\0';
            }
            //Only parse the target script selected by the user
            if (index != TargetScriptIndex)
            {
                index++;
                scriptRootXml = scriptRootXml->NextSiblingElement();
                continue;
            }

            //Get and read condition node
            auto* scriptConditionXml = scriptRootXml->FirstChildElement("condition");
            if (!scriptConditionXml)
                THROW_EXCEPTION("Failed to get <condition> node from scriptx <script> node \"{}\"", scriptRootXml->Value());

            //Create script node on graph and link to condition node
            auto* scriptRoot = AddNode("Script",
                { new StaticAttribute_String(scriptRootXml->GetText(), "Name"), new OutputAttribute_Run }
            )->SetCustomTitlebarColor(ScriptColor);

            //Read <script> contents and generate node graph from them
            tinyxml2::XMLElement* currentNode = scriptConditionXml;
            Node* currentGraphNode = scriptRoot;
            while (currentNode)
            {
                //Parse scriptx node and move to next sibling
                ParseScriptxNode(currentNode, currentGraphNode);
                currentNode = currentNode->NextSiblingElement();
            }

            index++;
            scriptRootXml = scriptRootXml->NextSiblingElement();
        }

        //Go to next group/managed block
        curGroup = curGroup->NextSiblingElement();
    }

    //Todo: Add ui selector for different group/managed blocks or draw labels around them and draw all nodes at once
    //Todo: Sort all nodes so their run attributes are first and their continue attributes are last
}

void ParseScriptxNode(tinyxml2::XMLElement* xmlNode, Node* graphParent)
{
    //Parse node
    string nodeTypeName(xmlNode->Value());
    string nodeText(xmlNode->GetText() ? xmlNode->GetText() : "Invalid");

    //Todo: Add helper for this. Repeated several times
    size_t newlinePos = nodeTypeName.find('\n');
    if(newlinePos != string::npos && newlinePos != nodeTypeName.size() - 1) //Clear extra data like tabs after newline
        nodeTypeName = nodeTypeName.erase(newlinePos);

    newlinePos = nodeText.find('\n');
    if (newlinePos != string::npos && newlinePos != nodeText.size() - 1) //Clear extra data like tabs after newline
        nodeText = nodeText.erase(newlinePos);

    ////Todo: Fix this in a less dumb way
    ////Dumb fix for strings being invalid
    //if (nodeTypeName.back() != '\n')
    //    nodeTypeName += '\n';
    //if (nodeText.back() != '\n')
    //    nodeText += '\n';
    
    //Todo: Have list of function + action definitions + their inputs and outputs + io names that'll be used here instead of just trusting the scriptx file
    if (nodeTypeName == "action")
    {
        Log->info("Reached <action> node");
        auto* newNode = AddNode(nodeText, { new InputAttribute_Run, new OutputAttribute_Run("Continue") });
        auto* newNodeInRunAttribute = newNode->GetAttribute(0);
        AddLink(graphParent->GetAttribute(1), newNodeInRunAttribute);

        //Parse child nodes and add attributes / other nodes based on them
        tinyxml2::XMLElement* xmlNodeChild = xmlNode->FirstChildElement();
        while (xmlNodeChild)
        {
            ParseScriptxNode(xmlNodeChild, newNode);
            xmlNodeChild = xmlNodeChild->NextSiblingElement();
        }
    }
    else if (nodeTypeName == "condition")
    {
        //Parse child nodes and add attributes / other nodes based on them
        tinyxml2::XMLElement* xmlNodeChild = xmlNode->FirstChildElement();
        while (xmlNodeChild)
        {
            ParseScriptxNode(xmlNodeChild, graphParent);
            xmlNodeChild = xmlNodeChild->NextSiblingElement();
        }
    }
    else if (nodeTypeName == "variable")
    {
        //Todo: Add support for these
        Log->info("Reached <variable> node");
    }
    else if (nodeTypeName == "variable_ref")
    {
        //Todo: Add support for these
        Log->info("Reached <variable_ref> node");
    }
    else if (nodeTypeName == "delay")
    {
        Log->info("Reached <delay> node");
        auto* newNode = AddNode("Delay", { new InputAttribute_Run, new OutputAttribute_Run("Continue") })->SetCustomTitlebarColor(DelayColor);
        auto* newNodeInRunAttribute = newNode->GetAttribute(0);
        AddLink(graphParent->GetAttribute(1), newNodeInRunAttribute);

        //Parse child nodes and add attributes / other nodes based on them
        tinyxml2::XMLElement* xmlNodeChild = xmlNode->FirstChildElement();
        while (xmlNodeChild)
        {
            ParseScriptxNode(xmlNodeChild, newNode);
            xmlNodeChild = xmlNodeChild->NextSiblingElement();
        }
    }
    if (nodeTypeName == "flag")
    {
        bool value = nodeText == "true" ? true : false;
        auto* newAttribute = graphParent->AddAttribute(new InputAttribute_Bool(value, "Flag"));
        //Todo: Add option to prefer separate nodes for flags or to prefer inline. For now just do inline by default for bools
        /*Node* childNode = AddNode("Flag", { new OutputAttribute_Bool });
        AddLink(childNode->GetAttribute(0), newAttribute);*/
    }
    else if (nodeTypeName == "function")
    {
        //Todo: Support other function types. Right now it assumes they all return a handle as a dumb filler for initial testing
        //Create function node
        auto* newFunctionNode = AddNode(nodeText,
            { new OutputAttribute_Handle }
        )->SetCustomTitlebarColor(FunctionColor);

        //Todo: Support types other than object handle here. Use function definitions list
        //Create new input attribute to take the function input
        auto* newAttribute = graphParent->AddAttribute(new InputAttribute_Handle);
        AddLink(newFunctionNode->GetAttribute(0), newAttribute);

        //Parse child nodes and add attributes / other nodes based on them
        tinyxml2::XMLElement* xmlNodeChild = xmlNode->FirstChildElement();
        while (xmlNodeChild)
        {
            ParseScriptxNode(xmlNodeChild, newFunctionNode);
            xmlNodeChild = xmlNodeChild->NextSiblingElement();
        }
    }
    else if (nodeTypeName == "event")
    {
        //Todo: Support other function types. Right now it assumes they all return a handle as a dumb filler for initial testing
        //Create function node
        auto* newFunctionNode = AddNode(nodeText,
            { new OutputAttribute_Handle }
        )->SetCustomTitlebarColor(EventColor);

        //Todo: Support types other than object handle here. Use function definitions list
        //Create new input attribute to take the function input
        auto* newAttribute = graphParent->AddAttribute(new InputAttribute_Handle);
        AddLink(newFunctionNode->GetAttribute(0), newAttribute);

        //Parse child nodes and add attributes / other nodes based on them
        tinyxml2::XMLElement* xmlNodeChild = xmlNode->FirstChildElement();
        while (xmlNodeChild)
        {
            ParseScriptxNode(xmlNodeChild, newFunctionNode);
            xmlNodeChild = xmlNodeChild->NextSiblingElement();
        }
    }
    else if (nodeTypeName == "anim_action")
    {
        //Todo: Change this to a radio dialog that has all the anim actions listed. May be able to get list from xtbls
        auto* newAttribute = graphParent->AddAttribute(new StaticAttribute_String(nodeText, "anim_action"));
    }
    else if (nodeTypeName == "string")
    {
        //Todo: Change this to a radio dialog that has all the anim actions listed. May be able to get list from xtbls
        auto* newAttribute = graphParent->AddAttribute(new StaticAttribute_String(nodeText, "String"));
    }
    else if (nodeTypeName == "mission_handle")
    {
        //Todo: Change this to a radio dialog that has all the anim actions listed. May be able to get list from xtbls
        auto* newAttribute = graphParent->AddAttribute(new StaticAttribute_String(nodeText, "Mission handle"));
    }
    else if (nodeTypeName == "music_threshold")
    {
        //Todo: Change this to a radio dialog that has all the anim actions listed. May be able to get list from xtbls
        auto* newAttribute = graphParent->AddAttribute(new StaticAttribute_String(nodeText, "music_threshold"));
    }
    else if (nodeTypeName == "voice_line")
    {
        //Todo: Change this to a radio dialog that has all the anim actions listed. May be able to get list from xtbls
        auto* newAttribute = graphParent->AddAttribute(new StaticAttribute_String(nodeText, "voice_line"));
    }
    else if (nodeTypeName == "object_list")
    {
        //Todo: Support
    }
    else if (nodeTypeName == "number" || nodeTypeName == "min" || nodeTypeName == "max")
    {
        //Todo: Use action/function definitions list to attempt to set name of argument here
        auto* newAttribute = graphParent->AddAttribute(new InputAttribute_Number(std::stof(nodeText), nodeTypeName));
    }
    else if (nodeTypeName == "object")
    {
        //Attempt to object_number node from within object node
        tinyxml2::XMLElement* objectNumberNode = xmlNode->FirstChildElement();
        string objectNumberNodeTypeName(objectNumberNode->Value());
        string objectNumberNodeText(objectNumberNode->GetText() ? objectNumberNode->GetText() : "Invalid");

        //Todo: Make a helper function for this. Being repeated all over
        size_t newlinePosObjectNumber = objectNumberNodeText.find('\n');
        if (newlinePosObjectNumber != string::npos) //Clear extra data like tabs after newline
            objectNumberNodeText = objectNumberNodeText.erase(newlinePosObjectNumber);

        if (objectNumberNodeTypeName == "object_number")
        {
            auto* newAttribute = graphParent->AddAttribute(new InputAttribute_Handle(std::stof(nodeText), std::stof(objectNumberNodeText)));
        }
    }
    else
    {
        Log->warn("Unknown script node type \"{}\" reached. Skipping.", xmlNode->Value());
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
    static bool needAutoLayout = false;
    static string targetScriptx = "terr01_tutorial.scriptx";
    static string targetScriptxBuffer = targetScriptx;
    //Todo: Have list of valid scriptx files here instead of a text input
    ImGui::InputText("Scriptx", &targetScriptxBuffer);
    ImGui::SameLine();
    if (ImGui::Button("Reload##ReloadTargetScriptx"))
    {
        SetTargetScript(0);
        ScriptxEditor_Cleanup(state);
        targetScriptx = targetScriptxBuffer;
        ScriptList = "";
        LoadScriptxFile(targetScriptx, state);
        needAutoLayout = true;
    }

    if (firstCall)
    {
        imnodes::Initialize();
        //Todo: Clear imnodes state and free attributes when finished with them
        firstCall = false;

        LoadScriptxFile(targetScriptx, state);
    }
    //Todo: Destroy imnodes context later

    static bool firstDraw = true;
    static ImVec2 gridPos = { 25.0f, 25.0f };

    static int currentItem = 0;
    if (ImGui::Combo("Script", &currentItem, (const char*)ScriptList.data()))
    {
        SetTargetScript(currentItem);
        ScriptxEditor_Cleanup(state);
        LoadScriptxFile(targetScriptx, state);
        needAutoLayout = true;
    }
    ImGui::Separator();
    
    imnodes::BeginNodeEditor();

    //Draw nodes
    for (auto& node : Nodes)
    {
        node->Draw();
        if (firstDraw || needAutoLayout)
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
                curPos.x += widestNodeFound * 2.0f;
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
    needAutoLayout = false;
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