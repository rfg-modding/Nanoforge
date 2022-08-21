using System.Collections;
using Nanoforge.App;
using Nanoforge;
using System;
using ImGui;

namespace Nanoforge.Gui.Panels
{
    //Has a bunch of info about the apps state, system functions, and stages
    public class StateViewer : IGuiPanel
    {
        void IGuiPanel.Update(App app)
        {
            if (!ImGui.Begin(scope String(Icons.ICON_FA_CODE_BRANCH)..Append(" App state")))
            {
                ImGui.End();
                return;
            }

            const f32 indent = 15.0f;
            if (ImGui.CollapsingHeader("State stack", .DefaultOpen))
            {
                ImGui.Indent(indent);
                for (int i in 0..<app.[Friend]_stateStack.Count)
                {
                    AppState state = app.[Friend]_stateStack[i];
                    ImGui.Bullet();
                    ImGui.SameLine();
                    if (i == app.[Friend]_stateStack.Count - 1) //Top of the state stack (active state)
                    {
                        ImGui.TextColored(Enum.ValueToString<AppState>(.. scope .(), state), ImGui.SecondaryTextColor);
                    }
                    else
                    {
                        ImGui.Text(Enum.ValueToString<AppState>(.. scope .(), state));
                    }
                }
                ImGui.Unindent(indent);
            }

            if (ImGui.CollapsingHeader("Systems", .DefaultOpen))
            {
                ImGui.Indent(indent);

                if (ImGui.BeginTable("##SystemsTable", 4, .ScrollY | .RowBg | .BordersOuter | .BordersV | .Resizable | .Reorderable | .Hideable | .SizingFixedFit, .(0.0f, 800.0f)))
                {
                    ImGui.TableSetupScrollFreeze(0, 1); //Make first column always visible
                    ImGui.TableSetupColumn("Name", .None);
                    ImGui.TableSetupColumn("Run type", .None);
                    ImGui.TableSetupColumn("Avg time (ms)", .None);
                    ImGui.TableSetupColumn("Size (bytes)", .None);
                    ImGui.TableHeadersRow();

                    ImGui.PushStyleVar(.IndentSpacing, ImGui.GetFontSize() * 2.0f);
                    ImGui.TableNextRow();
                    ImGui.TableNextColumn();
                    if (ImGui.TreeNodeEx("Startup", .DefaultOpen))
                    {
                        for (SystemFunction system in app.[Friend]_startupSystems)
                            DrawSystem(system);

                        ImGui.TreePop();
                    }

                    //Separator between startup systems and other systems. Startup systems are only run once while other systems are usually per frame
                    ImGui.TableNextRow();
                    ImGui.TableNextRow();
                    ImGui.TableNextColumn();
                    ImGui.Separator();
                    ImGui.TableNextColumn();
                    ImGui.Separator();
                    ImGui.TableNextColumn();
                    ImGui.Separator();
                    ImGui.TableNextColumn();
                    ImGui.Separator();

                    for (int stageIndex in 0 ..< Enum.Count<SystemStage>() - 1)
                    {
                        Stage stage = app.[Friend]_stages[stageIndex];
                        ImGui.TableNextRow();
                        ImGui.TableNextColumn();
                        if (ImGui.TreeNodeEx(Enum.ValueToString<SystemStage>(.. scope .(), (SystemStage)stageIndex), .DefaultOpen))
                        {
                            //Independent systems
                            if (stage.IndependentSystems.Count > 0)
                            {
                                ImGui.TableNextRow();
                                ImGui.TableNextColumn();
                                if (ImGui.TreeNodeEx("Independent systems", .DefaultOpen))
                                {
                                    for (var systemFunc in stage.IndependentSystems)
                                        DrawSystem(systemFunc);

                                    ImGui.TreePop();
                                }
                            }

                            //State dependendent systems
                            if (HasStateDependentSystems(stage))
                            {
                                ImGui.TableNextRow();
                                ImGui.TableNextColumn();
                                if (ImGui.TreeNodeEx("State dependent systems", .DefaultOpen))
                                {
                                    for (int state in 0..<stage.Sets.Count)
                                    {
                                        SystemSet systemSet = stage.Sets[state];
                                        String stateName = Enum.ValueToString<AppState>(.. scope .(), (AppState)state);

                                        if (HasSystems(systemSet))
                                            DrawStateSystems(app, systemSet, stateName);
                                    }
                                    ImGui.TreePop();
                                }
                            }

                            if (!HasSystems(stage))
                            {
                                ImGui.TableNextRow();
                                ImGui.TableNextColumn();
								ImGui.Text("No systems");
							}

                            ImGui.TreePop();
                        }
                    }
                    ImGui.PopStyleVar();
                    ImGui.EndTable();
                }

                ImGui.Unindent(indent);
            }

            if (ImGui.CollapsingHeader("Resources"))
            {
                ImGui.Indent(indent);

                if (ImGui.BeginTable("##ResourcesTable", 2, .ScrollY | .RowBg | .BordersOuter | .BordersV | .Resizable | .Reorderable | .Hideable | .SizingFixedFit, .(0.0f, 250.0f)))
                {
                    ImGui.TableSetupScrollFreeze(1, 0); //Make first column always visible
                    ImGui.TableSetupColumn("Name", .None);
                    ImGui.TableSetupColumn("Size (bytes)", .None);
                    ImGui.TableHeadersRow();

                    for (int i in 0 ..< app.[Friend]_resources.Count)
                    {
                        Type resourceType = app.[Friend]_resourceTypes[i];

                        ImGui.TableNextRow();
                        ImGui.TableNextColumn();
                        ImGui.Text(resourceType.GetName(.. scope .()));

                        ImGui.TableNextColumn();
                        ImGui.Text(scope $"{resourceType.InstanceSize}");
                    }

                    ImGui.EndTable();
                }

                ImGui.Unindent(indent);
            }

            ImGui.End();
        }

        //Draw tree node of state + systems in it
        void DrawStateSystems(App app, SystemSet systemSet, StringView stateName)
        {
            ImGui.TableNextRow();
            ImGui.TableNextColumn();
            if (ImGui.TreeNodeEx(stateName, .DefaultOpen))// | (anySystemsInThisState ? .None : .Leaf)))
            {
                for (int runType in 0..<systemSet.Systems.Count)
                {
                    List<SystemFunction> systemFunctions = systemSet.Systems[runType];
                    String runTypeName = Enum.ValueToString<SystemRunType>(.. scope .(), (SystemRunType)runType);

                    for (var systemFunc in systemFunctions)
                        DrawSystem(systemFunc, runTypeName);
                }

                ImGui.TreePop();
            }
        }

        void DrawSystem(SystemFunction systemFunc, String runTypeName = null)
        {
            ImGui.TableNextRow();
            ImGui.TableNextColumn();
            ImGui.TextColored(scope $"{systemFunc.ClassName}.", ImGui.TertiaryTextColor);
            ImGui.SameLine();
            ImGui.SetCursorPosX(ImGui.GetCursorPosX() - 8.0f);
            ImGui.TextColored(scope $"{systemFunc.Name}()", ImGui.SecondaryTextColor);

            ImGui.TableNextColumn();
            if (runTypeName != null)
                ImGui.Text(runTypeName);

            ImGui.TableNextColumn();
            ImGui.Text(scope String()..AppendF("{0:F3}", systemFunc.AverageRunTime() * 1000.0f));

            ImGui.TableNextColumn();
            ImGui.Text($"{systemFunc.[Friend]_system.GetType().InstanceSize}");
        }

        bool HasSystems(SystemSet systemSet)
        {
            for (List<SystemFunction> systems in systemSet.Systems)
                if (systems.Count > 0)
                    return true;

            return false;
        }

        bool HasSystems(Stage stage)
        {
            return stage.IndependentSystems.Count > 0 || HasStateDependentSystems(stage);
        }

        bool HasStateDependentSystems(Stage stage)
        {
            for (SystemSet systemSet in stage.Sets)
                if (HasSystems(systemSet))
                    return true;

            return false;
        }
    }
}