#pragma once
#include "gui/GuiState.h"

void ScriptxEditor_Update(GuiState* state)
{
    if (!ImGui::Begin("Scriptx editor"))
    {
        ImGui::End();
        return;
    }

    state->fontManager_->FontL.Push();
    ImGui::Text(ICON_FA_PROJECT_DIAGRAM "Scriptx editor");
    state->fontManager_->FontL.Pop();
    ImGui::Separator();

    node::SetCurrentEditor(state->nodeEditor_);

    node::Begin("My Editor");

    // Struct to hold basic information about connection between
// pins. Note that connection (aka. link) has its own ID.
// This is useful later with dealing with selections, deletion
// or other operations.
    struct LinkInfo
    {
        node::LinkId Id;
        node::PinId  InputId;
        node::PinId  OutputId;
    };

    u32 uniqueId = 1;
    static bool g_FirstFrame = true;
    static ImVector<LinkInfo>   g_Links;                // List of live links. It is dynamic unless you want to create read-only view over nodes.
    static int                  g_NextLinkId = 100;     // Counter to help generate link ids. In real application this will probably based on pointer to user data structure.

    //
    // 1) Commit known data to editor
    //

    // Submit Node A
    node::NodeId nodeA_Id = uniqueId++;
    node::PinId  nodeA_InputPinId = uniqueId++;
    node::PinId  nodeA_OutputPinId = uniqueId++;

    if (g_FirstFrame)
        node::SetNodePosition(nodeA_Id, ImVec2(10, 10));

    node::BeginNode(nodeA_Id);
    ImGui::Text("Node A");
    node::BeginPin(nodeA_InputPinId, node::PinKind::Input);
    ImGui::Text("-> In");
    node::EndPin();
    ImGui::SameLine();
    node::BeginPin(nodeA_OutputPinId, node::PinKind::Output);
    ImGui::Text("Out ->");
    node::EndPin();
    node::EndNode();

    // Submit Node B
    node::NodeId nodeB_Id = uniqueId++;
    node::PinId  nodeB_InputPinId1 = uniqueId++;
    node::PinId  nodeB_InputPinId2 = uniqueId++;
    node::PinId  nodeB_OutputPinId = uniqueId++;

    if (g_FirstFrame)
        node::SetNodePosition(nodeB_Id, ImVec2(210, 60));
    node::BeginNode(nodeB_Id);
    ImGui::Text("Node B");
    //ImGuiEx_BeginColumn();
    node::BeginPin(nodeB_InputPinId1, node::PinKind::Input);
    ImGui::Text("-> In1");
    node::EndPin();
    node::BeginPin(nodeB_InputPinId2, node::PinKind::Input);
    ImGui::Text("-> In2");
    node::EndPin();
    //ImGuiEx_NextColumn();
    node::BeginPin(nodeB_OutputPinId, node::PinKind::Output);
    ImGui::Text("Out ->");
    node::EndPin();
    //ImGuiEx_EndColumn();
    node::EndNode();

    // Submit Links
    for (auto& linkInfo : g_Links)
        node::Link(linkInfo.Id, linkInfo.InputId, linkInfo.OutputId);

    //
    // 2) Handle interactions
    //

    // Handle creation action, returns true if editor want to create new object (node or link)
    if (node::BeginCreate())
    {
        node::PinId inputPinId, outputPinId;
        if (node::QueryNewLink(&inputPinId, &outputPinId))
        {
            // QueryNewLink returns true if editor want to create new link between pins.
            //
            // Link can be created only for two valid pins, it is up to you to
            // validate if connection make sense. Editor is happy to make any.
            //
            // Link always goes from input to output. User may choose to drag
            // link from output pin or input pin. This determine which pin ids
            // are valid and which are not:
            //   * input valid, output invalid - user started to drag new ling from input pin
            //   * input invalid, output valid - user started to drag new ling from output pin
            //   * input valid, output valid   - user dragged link over other pin, can be validated

            if (inputPinId && outputPinId) // both are valid, let's accept link
            {
                // node::AcceptNewItem() return true when user release mouse button.
                if (node::AcceptNewItem())
                {
                    // Since we accepted new link, lets add one to our list of links.
                    g_Links.push_back({ node::LinkId(g_NextLinkId++), inputPinId, outputPinId });

                    // Draw new link.
                    node::Link(g_Links.back().Id, g_Links.back().InputId, g_Links.back().OutputId);
                }

                // You may choose to reject connection between these nodes
                // by calling node::RejectNewItem(). This will allow editor to give
                // visual feedback by changing link thickness and color.
            }
        }
    }
    node::EndCreate(); // Wraps up object creation action handling.


    // Handle deletion action
    if (node::BeginDelete())
    {
        // There may be many links marked for deletion, let's loop over them.
        node::LinkId deletedLinkId;
        while (node::QueryDeletedLink(&deletedLinkId))
        {
            // If you agree that link can be deleted, accept deletion.
            if (node::AcceptDeletedItem())
            {
                // Then remove link from your data.
                for (auto& link : g_Links)
                {
                    if (link.Id == deletedLinkId)
                    {
                        g_Links.erase(&link);
                        break;
                    }
                }
            }

            // You may reject link deletion by calling:
            // node::RejectDeletedItem();
        }
    }
    node::EndDelete(); // Wrap up deletion action



    node::End();

    g_FirstFrame = false;

    ImGui::End();
}