#pragma once
#include "common/Typedefs.h"
#include "imgui.h"
#include <spdlog/fmt/fmt.h>
#pragma warning(push) //Disable imnodes library warnings
#pragma warning(disable:26495)
#pragma warning(disable:26812)
#pragma warning(disable:6031)
#pragma warning(disable:4996)
#include "gui/panels/scriptx_editor/ScriptxEditor.h"
#include "Node.h"
#include <imnodes.h>
#pragma warning(pop)

enum AttributeType
{
    Input,
    Output,
    Static
};

struct IAttribute
{
    i32 Id = -1;
    Node* Parent = nullptr;
    AttributeType Type = Static;
    virtual void Draw(ScriptxEditor& editor) = 0;
    virtual ~IAttribute() { }
};

//Static attributes
struct StaticAttribute_String : IAttribute
{
    string Label;
    string Value;

    StaticAttribute_String(const string& value) : Label("String"), Value(value) { Type = Static; }
    StaticAttribute_String(const string& value, const string& label) : Label(label), Value(value) { Type = Static; }
#pragma warning(disable:4100)
    void Draw(ScriptxEditor& editor) override
    {
        imnodes::BeginStaticAttribute(Id);
        ImGui::SetNextItemWidth(200.0f);
        ImGui::InputText(Label.c_str(), &Value);
        imnodes::EndStaticAttribute();
    }
#pragma warning(default:4100)
};

//Input attributes
struct InputAttribute_Bool : IAttribute
{
    bool OverrideValue = false;
    string Label = "Flag";

    InputAttribute_Bool() { Type = Input; }
    InputAttribute_Bool(bool override, const string& label) : OverrideValue(override), Label(label) { Type = Input; }
    void Draw(ScriptxEditor& editor) override
    {
        imnodes::BeginInputAttribute(Id, imnodes::PinShape::PinShape_CircleFilled);
        ImGui::Text(Label);
        if (!editor.IsAttributeLinked(Id))
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
    void Draw(ScriptxEditor& editor) override
    {
        imnodes::BeginInputAttribute(Id, imnodes::PinShape::PinShape_CircleFilled);
        ImGui::Text(Label);
        if (!editor.IsAttributeLinked(Id))
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
#pragma warning(disable:4100)
    void Draw(ScriptxEditor& editor) override
    {
        imnodes::BeginInputAttribute(Id, imnodes::PinShape::PinShape_CircleFilled);
        ImGui::Text("String");
        imnodes::EndInputAttribute();
    }
#pragma warning(default:4100)
};
struct InputAttribute_Handle : IAttribute
{
    u32 ObjectClass = 0;
    u32 ObjectNumber = 0;

    InputAttribute_Handle() { Type = Input; }
    InputAttribute_Handle(u32 classnameHash, u32 num) : ObjectClass(classnameHash), ObjectNumber(num) { Type = Input; }
    void Draw(ScriptxEditor& editor) override
    {
        imnodes::BeginInputAttribute(Id, imnodes::PinShape::PinShape_CircleFilled);
        ImGui::Text("Handle");
        if (!editor.IsAttributeLinked(Id))
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
#pragma warning(disable:4100)
    void Draw(ScriptxEditor& editor) override
    {
        imnodes::BeginInputAttribute(Id, imnodes::PinShape::PinShape_TriangleFilled);
        ImGui::Text(Label.c_str());
        imnodes::EndInputAttribute();
    }
#pragma warning(default:4100)
};

//Output attributes
struct OutputAttribute_Bool : IAttribute
{
    bool Value = false;

    OutputAttribute_Bool() { Type = Output; }
#pragma warning(disable:4100)
    void Draw(ScriptxEditor& editor) override
    {
        imnodes::BeginOutputAttribute(Id, imnodes::PinShape::PinShape_CircleFilled);
        static bool flag = true;
        ImGui::Checkbox(fmt::format("Value##{}", Id).c_str(), &flag);
        imnodes::EndOutputAttribute();
    }
#pragma warning(default:4100)
};
struct OutputAttribute_Handle : IAttribute
{
    OutputAttribute_Handle() { Type = Output; }
#pragma warning(disable:4100)
    void Draw(ScriptxEditor& editor) override
    {
        imnodes::BeginOutputAttribute(Id, imnodes::PinShape::PinShape_CircleFilled);
        ImGui::Text("Handle");
        imnodes::EndOutputAttribute();
    }
#pragma warning(default:4100)
};
struct OutputAttribute_Run : IAttribute
{
    string Label;

    OutputAttribute_Run(const string& label = "Run") : Label(label) { Type = Output; }
#pragma warning(disable:4100)
    void Draw(ScriptxEditor& editor) override
    {
        imnodes::BeginOutputAttribute(Id, imnodes::PinShape::PinShape_TriangleFilled);
        ImGui::Text(Label.c_str());
        imnodes::EndOutputAttribute();
    }
#pragma warning(default:4100)
};

#define BOOL_NODE() AddNode("Bool", { new OutputAttribute_Bool }, {}, {});
#define SCRIPT_NODE(ScriptName) AddNode("Script", { new StaticAttribute_String(ScriptName) }, { new InputAttribute_Bool }, { new OutputAttribute_Run });