#pragma once
#include "gui/documents/XtblDocument.h"
#include "rfg/xtbl/XtblNodes.h"
#include "Common/Typedefs.h"
#include "XtblDescription.h"
#include "IXtblNode.h"
#include "XtblType.h"
#include "imgui.h"
#include "render/imgui/imgui_ext.h"
#include "render/imgui/ImGuiConfig.h"

//Node which references the value of nodes in another xtbl
class ReferenceXtblNode : public IXtblNode
{
public:
    void CollectReferencedNodes(GuiState* guiState, Handle<XtblFile> xtbl)
    {
        //Get referenced xtbl
        refXtbl_ = guiState->Xtbls->GetOrCreateXtbl(xtbl->VppName, desc_->Reference->File);
        if (!refXtbl_)
            return;

        //Continue if the node has a description and the reference list needs to be regenerated
        bool needRecollect = refXtbl_->Entries.size() != referencedNodes_.size();
        if (!desc_->Reference || !needRecollect)
            return;

        //Clear references list if it's being regenerated
        if (referencedNodes_.size() > 0)
            referencedNodes_.clear();

        //Find referenced nodes
        auto split = String::SplitString(desc_->Reference->Path, "/");
        string optionPath = desc_->Reference->Path.substr(desc_->Reference->Path.find_first_of('/') + 1);
        for (auto& subnode : refXtbl_->Entries)
        {
            if (!String::EqualIgnoreCase(subnode->Name, split[0]))
                continue;

            //Get list of matching subnodes. Some files like human_team_names.xtbl use lists instead of separate elements
            auto options = refXtbl_->GetSubnodes(optionPath, subnode);
            if (options.size() == 0)
                continue;

            for (auto& option : options)
                referencedNodes_.push_back(option);
        }

        //Find largest reference name to align combo buttons
        for (auto& node : referencedNodes_)
        {
            ImVec2 size = ImGui::CalcTextSize(std::get<string>(node->Value).c_str());
            if (maxReferenceSize_.x < size.x)
                maxReferenceSize_.x = size.x;
            if (maxReferenceSize_.y < size.y)
                maxReferenceSize_.y = size.y;
        }

        collectedReferencedNodes_ = true;
    }

    virtual void DrawEditor(GuiState* guiState, Handle<XtblFile> xtbl, IXtblNode* parent, const char* nameOverride = nullptr)
    {
        //Calculate cached data
        CalculateEditorValues(xtbl, nameOverride);
        CollectReferencedNodes(guiState, xtbl);

        //Check if the reference type is supported
        bool supported = referencedNodes_.size() > 0 && (
            //String
            referencedNodes_[0]->Type == XtblType::String ||
            //Reference to a reference to a string
            (referencedNodes_[0]->Type == XtblType::Reference && std::holds_alternative<string>(referencedNodes_[0]->Value)));

        //Handle errors
        if (!desc_->Reference)
        {
            gui::LabelAndValue(nameNoId_.value() + ":", std::get<string>(Value) + " [Error: Null reference]");
            return;
        }
        if (!refXtbl_)
        {
            //If referenced xtbl isn't found allow manual editing but report the error
            if (std::holds_alternative<string>(Value) && ImGui::InputText(name_.value(), std::get<string>(Value)))
                Edited = true;

            ImGui::SameLine();
            ImGui::TextColored(gui::SecondaryTextColor, " [Error: " + desc_->Reference->File + " not found! Cannot fill reference list.]");
            return;
        }
        if (!supported)
        {
            gui::LabelAndValue(nameNoId_.value() + ":", std::get<string>(Value) + " [Error: Unsupported reference type]");
            return;
        }

        //Get node value. Select first reference if the value hasn't been set yet
        string& nodeValue = std::get<string>(Value);
        if (nodeValue == "")
            nodeValue = std::get<string>(referencedNodes_[0]->Value);

        //Draw combo with an option for each referenced value
        if (ImGui::BeginCombo(name_.value().c_str(), std::get<string>(Value).c_str()))
        {
            //Draw search bar
            ImGui::InputText("Search", searchTerm_);
            ImGui::Separator();

            for (auto& option : referencedNodes_)
            {
                //Get option value
                string variableValue = std::get<string>(option->Value);
                bool selected = variableValue == nodeValue;

                //Check if option matches seach term
                if (searchTerm_ != "" && !String::Contains(String::ToLower(variableValue), String::ToLower(searchTerm_)))
                    continue;

                //Draw option
                if (ImGui::Selectable(variableValue.c_str(), &selected, 0, maxReferenceSize_))
                {
                    nodeValue = variableValue;
                    Edited = true;
                }

                //Draw button to jump to xtbl being referenced
                ImGui::SameLine();
                ImGui::SetCursorPos({ ImGui::GetCursorPosX(), ImGui::GetCursorPosY() + comboButtonOffsetY_ });
                if (ImGui::Button(fmt::format("{}##{}", ICON_FA_EXTERNAL_LINK_ALT, (u64)option).c_str(), ImVec2(comboButtonWidth_, comboButtonHeight_)))
                {
                    //Find parent element of variable
                    IXtblNode* documentRoot = option;
                    while (String::Contains(documentRoot->GetPath(), "/") && documentRoot->Parent)
                    {
                        documentRoot = documentRoot->Parent;
                    }

                    //Jump to referenced xtbl and select current node
                    auto document = std::dynamic_pointer_cast<XtblDocument>(guiState->GetDocument(refXtbl_->Name));
                    if (document)
                    {
                        document->SelectedNode = documentRoot;
                        ImGui::SetWindowFocus(document->Title.c_str());
                    }
                    else //Create new xtbl document if needed
                    {
                        document = CreateHandle<XtblDocument>(guiState, refXtbl_->Name, refXtbl_->VppName, refXtbl_->VppName, false, documentRoot);
                        guiState->CreateDocument(refXtbl_->Name, document);
                    }
                }
            }

            ImGui::EndCombo();
        }
    }

    virtual void InitDefault()
    {

    }

    virtual bool WriteModinfoEdits(tinyxml2::XMLElement* xml)
    {
        return WriteXml(xml, false);
    }

    virtual bool WriteXml(tinyxml2::XMLElement* xml, bool writeNanoforgeMetadata)
    {
        auto* referenceXml = xml->InsertNewChildElement(Name.c_str());
        referenceXml->SetText(std::get<string>(Value).c_str());

        //Store edited state if metadata is enabled
        if (writeNanoforgeMetadata)
        {
            referenceXml->SetAttribute("__NanoforgeEdited", Edited);
            referenceXml->SetAttribute("__NanoforgeNewEntry", NewEntry);
        }

        return true;
    }

private:
    //Search string used in reference list
    string searchTerm_ = "";

    //Cached list of nodes being referenced
    bool collectedReferencedNodes_ = false;
    Handle<XtblFile> refXtbl_ = nullptr;
    std::vector<IXtblNode*> referencedNodes_ = {}; //Referenced nodes in another xtbl

    //Values used to align reference with buttons that jump to their xtbl
    ImVec2 maxReferenceSize_ = { 0.0f, 0.0f };
    const f32 comboButtonWidth_ = 26.5f;
    const f32 comboButtonHeight_ = 25.0f;
    const f32 comboButtonOffsetY_ = -5.0f;
};