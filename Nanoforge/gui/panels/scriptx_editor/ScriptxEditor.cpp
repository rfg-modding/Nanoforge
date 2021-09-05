#include "ScriptxEditor.h"
#include "render/imgui/imgui_ext.h"
#include <types/Vec2.h>
#include "Node.h"
#include "Link.h"
#include "render/imgui/ImGuiConfig.h"
#include "imnodes.h"
#include <functional>
#include <spdlog/fmt/fmt.h>
#include "Log.h"
#include "util/Profiler.h"

ScriptxEditor::ScriptxEditor(GuiState* state)
{
    state_ = state;
}

ScriptxEditor::~ScriptxEditor()
{
    ScriptxEditor_Cleanup(state_);
}

void ScriptxEditor::Update(GuiState* state, bool* open)
{
    PROFILER_FUNCTION();
    ImGui::SetNextWindowSize({ 400.0f, 400.0f }, ImGuiCond_Appearing);
    if (!ImGui::Begin("Scriptx viewer", open))
    {
        ImGui::End();
        return;
    }

    state->FontManager->FontL.Push();
    ImGui::Text(ICON_FA_PROJECT_DIAGRAM "Scriptx viewer");
    state->FontManager->FontL.Pop();
    ImGui::Separator();

    static bool firstCall = true;
    static bool needAutoLayout = false;
    static string targetScriptxList = ""; //All scriptx names separated by null terminators. Dumb way of doing this but ImGui likes it
    static int targetScriptxCurrentItem = 0;
    //Lambda to extract target script name from the list of scriptx files
    auto getTargetScriptxName = [&]() -> string
    {
        Log->info("targetScriptxCurrentItem: {}\n", targetScriptxCurrentItem);
        string targetScriptxName = "";
        if (targetScriptxCurrentItem == 0)
        {
            targetScriptxName = targetScriptxList.substr(0, targetScriptxList.find_first_of('\0'));
        }
        else
        {
            u32 startPos = 0;
            u32 endPos = 0;
            u32 currentPos = 0;
            for (u32 i = 0; i <= targetScriptxCurrentItem; i++)
            {
                while (targetScriptxList[currentPos] != '\0')
                    currentPos++;
                if (targetScriptxList[currentPos] == '\0')
                    currentPos++;

                if (i == targetScriptxCurrentItem - 1)
                {
                    startPos = currentPos;
                }
                else if (i == targetScriptxCurrentItem)
                {
                    endPos = currentPos - 1;
                }
            }
            targetScriptxName = targetScriptxList.substr(startPos, endPos - startPos);
        }
        return targetScriptxName;
    };

    if (firstCall)
    {
        auto scriptxFiles = state->PackfileVFS->GetFiles("*.scriptx", false, false);
        for (auto& scriptx : scriptxFiles)
            targetScriptxList += scriptx.Filename() + '\0';
    }
    if (ImGui::Combo("Scriptx file", &targetScriptxCurrentItem, (const char*)targetScriptxList.data()))
    {
        SetTargetScript(0);
        ScriptxEditor_Cleanup(state);

        scriptList_ = "";
        string targetScriptx = getTargetScriptxName();
        LoadScriptxFile(targetScriptx, state);
        needAutoLayout = true;
    }

    if (firstCall)
    {
        imnodes::Initialize();
        //Todo: Clear imnodes state and free attributes when finished with them
        firstCall = false;

        string targetScriptx = getTargetScriptxName();
        LoadScriptxFile(targetScriptx, state);
    }
    //Todo: Destroy imnodes context later

    static bool firstDraw = true;
    static ImVec2 gridPos = { 25.0f, 25.0f };

    static int currentItem = 0;
    if (ImGui::Combo("Script", &currentItem, (const char*)scriptList_.data()))
    {
        SetTargetScript(currentItem);
        ScriptxEditor_Cleanup(state);
        string targetScriptx = getTargetScriptxName();
        LoadScriptxFile(targetScriptx, state);
        needAutoLayout = true;
    }
    ImGui::Separator();

    imnodes::BeginNodeEditor();

    //Draw nodes
    for (auto& node : nodes_)
    {
        node->Draw(*this);
        if (firstDraw || needAutoLayout)
        {
            u32 maxDepth = GetHighestNodeDepth();
            ImVec2 curPos = { 25.0f, 25.0f };
            for (u32 depth = 0; depth <= maxDepth; depth++)
            {
                f32 widestNodeFound = 0.0f;
                u32 depthCount = 0;
                for (auto* node : nodes_)
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
    for (const Link& link : links_)
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
            if (!startNode || !endNode)
                THROW_EXCEPTION("Scriptx link created for attribute with no owner!");

            link.Id = ++currentId_;
            link.StartNode = startNode;
            link.EndNode = endNode;
            links_.push_back(link);
        }
    }
    //Remove destroyed links from Links
    {
        int link_id;
        if (imnodes::IsLinkDestroyed(&link_id))
        {
            auto iter = std::find_if(
                links_.begin(), links_.end(), [link_id](const Link& link) -> bool {
                    return link.Id == link_id;
                });
            assert(iter != links_.end());
            links_.erase(iter);
        }
    }

    ImGui::End();
}

void ScriptxEditor::ScriptxEditor_Cleanup(GuiState* state)
{
    //Free nodes and attributes. They're allocated on the heap so they're pointers are stable
    for (Node* node : nodes_)
    {
        for (IAttribute* attribute : node->Attributes)
        {
            delete attribute;
        }
        delete node;
    }

    //Clear node and link vectors
    nodes_.clear();
    links_.clear();
}

void ScriptxEditor::LoadScriptxFile(const string& name, GuiState* state)
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
        THROW_EXCEPTION("Failed to get root node in scriptx file \"{}\"", name);

    //Todo: Read version, modified, and vars
    //Find all <managed> and <group> blocks. Parse <script> blocks within them.
    auto* curGroup = root->FirstChildElement();
    bool generateScriptList = scriptList_ == ""; //Only gen script list if it's empty
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

                scriptList_ += scriptName + '\0';
            }
            //Only parse the target script selected by the user
            if (index != targetScriptIndex_)
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

    delete[] scriptxBytes.data();

    //Todo: Add ui selector for different group/managed blocks or draw labels around them and draw all nodes at once
    //Todo: Sort all nodes so their run attributes are first and their continue attributes are last
}

void ScriptxEditor::ParseScriptxNode(tinyxml2::XMLElement* xmlNode, Node* graphParent)
{
    //Parse node
    string nodeTypeName(xmlNode->Value());
    string nodeText(xmlNode->GetText() ? xmlNode->GetText() : "Invalid");

    //Todo: Add helper for this. Repeated several times
    size_t newlinePos = nodeTypeName.find('\n');
    if (newlinePos != string::npos && newlinePos != nodeTypeName.size() - 1) //Clear extra data like tabs after newline
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
        auto* newAttribute = graphParent->AddAttribute(new InputAttribute_Bool(value, "Flag"), currentId_++);
        //Todo: Add option to prefer separate nodes for flags or to prefer inline. For now just do inline by default for bools
        /*Node* childNode = AddNode("Flag", { new OutputAttribute_Bool });
        AddLink(childNode->GetAttribute(0), newAttribute);*/
    }
    else if (nodeTypeName == "function")
    {
        //Todo: Support other function types. Right now it assumes they all return a handle as a dumb filler for initial testing
        //Create function node
        IAttribute* outputNode = (nodeText == "and" || nodeText == "or") ? (IAttribute*)new OutputAttribute_Bool : (IAttribute*)new OutputAttribute_Handle;
        auto* newFunctionNode = AddNode(nodeText,
            { outputNode }
        )->SetCustomTitlebarColor(FunctionColor);

        //Todo: Support types other than object handle here. Use function definitions list
        //Create new input attribute to take the function input
        auto* newAttribute = graphParent->AddAttribute(new InputAttribute_Handle, currentId_++);
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
        auto* newAttribute = graphParent->AddAttribute(new InputAttribute_Handle, currentId_++);
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
        auto* newAttribute = graphParent->AddAttribute(new StaticAttribute_String(nodeText, "anim_action"), currentId_++);
    }
    else if (nodeTypeName == "string")
    {
        //Todo: Change this to a radio dialog that has all the anim actions listed. May be able to get list from xtbls
        auto* newAttribute = graphParent->AddAttribute(new StaticAttribute_String(nodeText, "String"), currentId_++);
    }
    else if (nodeTypeName == "mission_handle")
    {
        //Todo: Change this to a radio dialog that has all the anim actions listed. May be able to get list from xtbls
        auto* newAttribute = graphParent->AddAttribute(new StaticAttribute_String(nodeText, "Mission handle"), currentId_++);
    }
    else if (nodeTypeName == "music_threshold")
    {
        //Todo: Change this to a radio dialog that has all the anim actions listed. May be able to get list from xtbls
        auto* newAttribute = graphParent->AddAttribute(new StaticAttribute_String(nodeText, "music_threshold"), currentId_++);
    }
    else if (nodeTypeName == "voice_line")
    {
        //Todo: Change this to a radio dialog that has all the anim actions listed. May be able to get list from xtbls
        auto* newAttribute = graphParent->AddAttribute(new StaticAttribute_String(nodeText, "voice_line"), currentId_++);
    }
    else if (nodeTypeName == "object_list")
    {
        //Todo: Support
    }
    else if (nodeTypeName == "number" || nodeTypeName == "min" || nodeTypeName == "max")
    {
        //Todo: Use action/function definitions list to attempt to set name of argument here
        auto* newAttribute = graphParent->AddAttribute(new InputAttribute_Number(std::stof(nodeText), nodeTypeName), currentId_++);
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
            auto* newAttribute = graphParent->AddAttribute(new InputAttribute_Handle(HexStringToUint(nodeText), std::stoul(objectNumberNodeText)), currentId_++);
        }
    }
    else
    {
        Log->warn("Unknown script node type \"{}\" reached. Skipping.", xmlNode->Value());
    }
}

Node* ScriptxEditor::AddNode(const string& title, const std::vector<IAttribute*> attributes)
{
    nodes_.push_back(new Node);
    Node& node = *nodes_.back();
    node.Id = currentId_++;
    node.Title = title;

    for (IAttribute* attribute : attributes)
        node.AddAttribute(attribute, currentId_++);

    return &node;
}

void ScriptxEditor::AddLink(IAttribute* start, IAttribute* end)
{
    links_.push_back({ currentId_++, start->Id, end->Id, start->Parent, end->Parent });
    start->Parent->AddSubnode(end->Parent);

    //If node already has a depth it creates better looking graphs to just walk starts depth back one from end rather than set it to 0
    if (end->Parent->Depth != 0)
        start->Parent->Depth = end->Parent->Depth - 1;

    start->Parent->UpdateSubnodeDepths();
}

bool ScriptxEditor::IsAttributeLinked(int id)
{
    for (auto& link : links_)
    {
        if (link.Start == id || link.End == id)
            return true;
    }

    return false;
}

Node* ScriptxEditor::FindAttributeOwner(i32 attributeId)
{
    for (auto* node : nodes_)
    {
        for (auto* attribute : node->Attributes)
        {
            if (attribute->Id == attributeId)
                return node;
        }
    }

    return nullptr;
}

u32 ScriptxEditor::GetHighestNodeDepth()
{
    u32 max = 0;
    for (auto* node : nodes_)
    {
        if (node->Depth > max)
            max = node->Depth;
    }
    return max;
}

void ScriptxEditor::SetTargetScript(u32 newTargetIndex)
{
    targetScriptIndex_ = newTargetIndex;
}

u32 ScriptxEditor::HexStringToUint(string str)
{
    u32 output;
    std::stringstream ss;
    ss << std::hex << str;
    ss >> output;
    return output;
}
