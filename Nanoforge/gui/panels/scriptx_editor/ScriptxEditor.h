#pragma once
#include "gui/GuiState.h"
#include "gui/IGuiPanel.h"
#include <types/Vec4.h>
#include <tinyxml2/tinyxml2.h>

struct Node;
struct Link;
struct IAttribute;

class ScriptxEditor : public IGuiPanel
{
public:
    ScriptxEditor(GuiState* state);
    ~ScriptxEditor();

    void Update(GuiState* state, bool* open) override;
    bool IsAttributeLinked(int id);

    const Vec4 DelayColor{ 130, 129, 27, 255 }; //Yellow
    const Vec4 PrimitiveColor{ 47, 141, 125, 255 }; //Light green
    const Vec4 ScriptColor{ 255, 129, 27, 255 }; //Orange
    const Vec4 ActionColor{ 33, 106, 183, 255 }; //Blue
    const Vec4 FunctionColor{ 181, 34, 75, 255 }; //Red
    const Vec4 EventColor{ 97, 47, 140, 255 }; //Purple

private:
    //Clear resources created by scriptx editor & node graph
    void Cleanup();
    //Load a scriptx file and generate a node graph from it. Clears any existing data/graphs before loading new one
    void LoadScriptxFile(const string& name, GuiState* state);
    void ParseScriptxNode(tinyxml2::XMLElement* xmlNode, Node* graphParent);
    Node* AddNode(const string& title, const std::vector<IAttribute*> attributes);
    void AddLink(IAttribute* start, IAttribute* end);
    Node* FindAttributeOwner(i32 attributeId);
    u32 GetHighestNodeDepth();
    void SetTargetScript(u32 newTargetIndex);
    u32 HexStringToUint(string str);

    i32 currentId_ = 0;
    //Note: Using Node* here so we can easily have stable refs/ptrs to nodes
    std::vector<Node*> nodes_;
    std::vector<Link> links_;
    string scriptList_ = "";
    u32 targetScriptIndex_ = 0;
    GuiState* state_ = nullptr;
};