#pragma once
#include "common/Typedefs.h"
#include "Attributes.h"
#include <types/Vec4.h>
#include <vector>
#include "gui/panels/scriptx_editor/ScriptxEditor.h"
#include "Log.h"

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
    IAttribute* AddAttribute(IAttribute* attribute, i32 id)
    {
        attribute->Id = id;
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
    void Draw(ScriptxEditor& editor)
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
            attribute->Draw(editor);

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
            THROW_EXCEPTION("Invalid scriptx attribute index.");

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