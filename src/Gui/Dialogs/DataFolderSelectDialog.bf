using Common;
using System;
using Nanoforge.App;
using ImGui;
using System.Collections;
using NativeFileDialog;
using Nanoforge.Misc;
using System.IO;
using Nanoforge.FileSystem;
using System.Diagnostics;
using Common.Math;

namespace Nanoforge.Gui.Dialogs
{
    //Used to select the RFG data folder and validate that it's correct.
    //Automatically shown by Gui if no data folder has been selected or it doesn't exist.
	public class DataFolderSelectDialog : Dialog
	{
        public append String DataFolder;
        private bool _showPathSelectorError = false;
        private append List<String> _searchResults = .() ~ClearAndDeleteItems(_);
        private append List<String> _validationWarnings = .() ~ClearAndDeleteItems(_);

        public this() : base("Data folder selector")
        {

        }

        public void Show()
        {
            Open = true;
            _firstDraw = true;
            Result = .None;
            DataFolder.Set("");
            _validationWarnings.ClearAndDeleteItems();
            _searchResults.ClearAndDeleteItems();

            //Auto search for RFG installs and populate list of results
            AutoDetectRfg();
            if (_searchResults.Count == 1)
            {
                DataFolder.Set(_searchResults[0]);
            }
        }

        public override void Draw(App app, Gui gui)
        {
            if (Open)
                ImGui.OpenPopup(Title);
            else
                return;

            defer { _firstDraw = false; }

            //Manually set padding since the parent window might be a document with padding disabled
            ImGui.PushStyleVar(.WindowPadding, .(8.0f, 8.0f));

            //Auto center
            ImGui.IO* io = ImGui.GetIO();
            ImGui.SetNextWindowPos(.(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), .Always, .(0.5f, 0.5f));

            if (ImGui.BeginPopupModal(Title, &Open, .AlwaysAutoResize | .NoMove | .NoNav | .NoCollapse))
            {
                ImGui.PushItemWidth(400.0f);

                //Auto detect + results
                ImGui.Text("Detected installs:");
                ImGui.SameLine();
                if (ImGui.SmallButton("Scan"))
                {
                    AutoDetectRfg();
                    if (_searchResults.Count == 1)
                    {
                        DataFolder.Set(_searchResults[0]);
                    }
                }
                f32 itemHeight = ImGui.GetTextLineHeightWithSpacing();
                if (ImGui.BeginChildFrame(ImGui.GetID("AutoDetectedRfgInstalls"), .(-f32.Epsilon, 4.25f * itemHeight)))
                {
                    if (_searchResults.Count > 0)
                    {
                        //Set custom highlight colors for the table
                        Vec4 selectedColor = .(0.157f, 0.350f, 0.588f, 1.0f);
                        Vec4 highlightedColor = selectedColor * 1.1f;
                        ImGui.PushStyleColor(.Header, selectedColor);
                        ImGui.PushStyleColor(.HeaderHovered, highlightedColor);
                        ImGui.PushStyleColor(.HeaderActive, highlightedColor);
                        for (String path in _searchResults)
                        {
                            if (ImGui.Selectable(path))
                            {
                                DataFolder.Set(path);
                                ValidateDataFolder();
                            }
                        }
                        ImGui.PopStyleColor(3);
                    }
                    else
                    {
                        ImGui.Text("No RFG installs found");
                    }  
                    ImGui.EndChildFrame();
                }

                ImGui.Separator();

                //Validation warnings
                if (_validationWarnings.Count > 0)
                {
                    ImGui.Text("Warnings:");
                    if (ImGui.BeginChildFrame(ImGui.GetID("DataFolderValidationWarnings"), .(-f32.Epsilon, 6.25f * itemHeight)))
                    {
                        for (String warning in _validationWarnings)
                        {
                            ImGui.TextColored(.Yellow, warning);
                        }
                        ImGui.EndChildFrame();
                    }
                }

                //Data folder selector
                ImGui.InputText("Data folder", DataFolder);
                ImGui.SameLine();
                if (ImGui.Button("..."))
                {
                    char8* outPath = null;
                    switch (NativeFileDialog.PickFolder(null, &outPath))
                    {
                        case .Okay:
                            DataFolder.Set(StringView(outPath));
                            ValidateDataFolder();
                            _showPathSelectorError = false;
                        case .Cancel:
                            _showPathSelectorError = false;
                        case .Error:
                            char8* error = NativeFileDialog.GetError();
                            Logger.Error("Error opening folder selector in new project dialog: {}", StringView(error));
                            _showPathSelectorError = true;
                    }
                }
                if (_showPathSelectorError)
                {
                    ImGui.SameLine();
                    ImGui.TextColored(.Red, "An error occurred while browsing for the data folder");
                }

                //Confirm/cancel buttons. Can use as long as the data folder is set. Doesn't matter if it's correct for the time being. Can't account for how modders might alter their data folder.
                ImGui.Separator();
                bool validDataFolder = !DataFolder.IsEmpty && !DataFolder.IsWhiteSpace;
                ImGui.BeginDisabled(!validDataFolder);
                if (ImGui.Button("Confirm"))
                {
                    PackfileVFS.InitFromDirectory("//data/", DataFolder);
                    Close();
                }
                ImGui.SameLine();
                if (ImGui.Button("Cancel"))
                {
                    Close();
                }
                ImGui.EndDisabled();

                ImGui.EndPopup();
            }
            ImGui.PopStyleVar();
        }

        public override void Close(Nanoforge.Gui.DialogResult result = .None)
        {
            Open = false;
            Result = result;
            _firstDraw = true;
            ImGui.CloseCurrentPopup();
        }

        private void ValidateDataFolder()
        {
            //Files that are in a normal RFG data folder
            static StringView[?] expectedFiles =
			.(
				"activities.vpp_pc", "anims.vpp_pc", "chunks.vpp_pc", "cloth_sim.vpp_pc", "decals.vpp_pc", "dlc01_l0.vpp_pc", "dlc01_l1.vpp_pc", "dlc01_precache.vpp_pc",
                "dlcp01_activities.vpp_pc", "dlcp01_anims.vpp_pc", "dlcp01_cloth_sim.vpp_pc", "dlcp01_effects.vpp_pc", "dlcp01_humans.vpp_pc", "dlcp01_interface.vpp_pc",
                "dlcp01_items.vpp_pc", "dlcp01_misc.vpp_pc", "dlcp01_missions.vpp_pc", "dlcp01_personas_en_us.vpp_pc", "dlcp01_vehicles_r.vpp_pc", "dlcp01_voices_en_us.vpp_pc",
                "dlcp02_interface.vpp_pc", "dlcp02_misc.vpp_pc", "dlcp03_interface.vpp_pc", "dlcp03_misc.vpp_pc", "effects.vpp_pc", "effects_mp.vpp_pc", "humans.vpp_pc",
                "interface.vpp_pc", "items.vpp_pc", "items_mp.vpp_pc", "misc.vpp_pc", "missions.vpp_pc", "mp_common.vpp_pc", "mp_cornered.vpp_pc", "mp_crashsite.vpp_pc",
                "mp_crescent.vpp_pc", "mp_crevice.vpp_pc", "mp_deadzone.vpp_pc", "mp_downfall.vpp_pc", "mp_excavation.vpp_pc", "mp_fallfactor.vpp_pc", "mp_framework.vpp_pc",
                "mp_garrison.vpp_pc", "mp_gauntlet.vpp_pc", "mp_overpass.vpp_pc", "mp_pcx_assembly.vpp_pc", "mp_pcx_crossover.vpp_pc", "mp_pinnacle.vpp_pc", "mp_quarantine.vpp_pc",
                "mp_radial.vpp_pc", "mp_rift.vpp_pc", "mp_sandpit.vpp_pc", "mp_settlement.vpp_pc", "mp_warlords.vpp_pc", "mp_wasteland.vpp_pc", "mp_wreckage.vpp_pc", "mpdlc_broadside.vpp_pc",
				"mpdlc_division.vpp_pc", "mpdlc_islands.vpp_pc", "mpdlc_landbridge.vpp_pc", "mpdlc_minibase.vpp_pc", "mpdlc_overhang.vpp_pc", "mpdlc_puncture.vpp_pc", "mpdlc_ruins.vpp_pc",
                "personas_de_de.vpp_pc", "personas_en_us.vpp_pc", "personas_es_es.vpp_pc", "personas_fr_fr.vpp_pc", "personas_it_it.vpp_pc", "personas_ru_ru.vpp_pc", "skybox.vpp_pc",
                "sounds_r.vpp_pc", "steam.vpp_pc", /*"table.vpp_pc", //Not checked since it's required to rename or delete this for misc.vpp_pc mods to work*/ "terr01_l0.vpp_pc",
                "terr01_l1.vpp_pc", "terr01_precache.vpp_pc", "vehicles_r.vpp_pc", "voices_de_de.vpp_pc", "voices_en_us.vpp_pc", "voices_es_es.vpp_pc", "voices_fr_fr.vpp_pc",
                "voices_it_it.vpp_pc", "voices_ru_ru.vpp_pc", "wc1.vpp_pc", "wc10.vpp_pc", "wc2.vpp_pc", "wc3.vpp_pc", "wc4.vpp_pc", "wc5.vpp_pc", "wc6.vpp_pc",
                "wc7.vpp_pc", "wc8.vpp_pc", "wc9.vpp_pc", "wcdlc1.vpp_pc", "wcdlc2.vpp_pc", "wcdlc3.vpp_pc", "wcdlc4.vpp_pc", "wcdlc5.vpp_pc", "wcdlc6.vpp_pc",
                "wcdlc7.vpp_pc", "wcdlc8.vpp_pc", "wcdlc9.vpp_pc", "zonescript_dlc01.vpp_pc", "zonescript_terr01.vpp_pc"
			);

            _validationWarnings.ClearAndDeleteItems();
            for (StringView filename in expectedFiles)
            {
                if (!File.Exists(scope $"{DataFolder}\\{filename}"))
                {
                    _validationWarnings.Add(new $"{filename} is missing");
                }
            }

            //TODO: Add additional checks in data folder parent. Can look for rfg.exe and other files and do validation hashes on them (assumes modders don't have separate data folder for modding)
        }

        [Trace(.RunTime)]
        private void AutoDetectRfg()
        {
            static StringView[?] searchTemplates =
			.(
                "{}:/Steam/steamapps/common/Red Faction Guerrilla Re-MARS-tered/data/",
                "{}:/SteamLibrary/steamapps/common/Red Faction Guerrilla Re-MARS-tered/data/",
                "{}:/GOG/Games/Red Faction Guerrilla Re-MARS-tered/data/",
                "{}:/Program Files (x86)/Steam/steamapps/common/Red Faction Guerrilla Re-MARS-tered/data/",
                "{}:/Program Files (x86)/SteamLibrary/steamapps/common/Red Faction Guerrilla Re-MARS-tered/data/",
                "{}:/Program Files (x86)/GOG/Games/Red Faction Guerrilla Re-MARS-tered/data/",
                "{}:/Program Files/Steam/steamapps/common/Red Faction Guerrilla Re-MARS-tered/data/",
                "{}:/Program Files/SteamLibrary/steamapps/common/Red Faction Guerrilla Re-MARS-tered/data/",
                "{}:/Program Files/GOG/Games/Red Faction Guerrilla Re-MARS-tered/data/",
                "{}:Games/SteamLibrary/steamapps/common/Red Faction Guerrilla Re-MARS-tered/data/",
                "{}:Games/GOG Games/Red Faction Guerrilla Re-MARS-tered/data",
                "{}:Games/GOG/Red Faction Guerrilla Re-MARS-tered/data",
			);

            _searchResults.ClearAndDeleteItems();
            for (char8 driveLetter in (int)'A' ... (int)'Z')
            {
                for (StringView template in searchTemplates)
                {
                    String searchPath = scope String()..AppendF(template, driveLetter);
                    if (Directory.Exists(searchPath))
                    {
                        _searchResults.Add(new String()..Set(searchPath));
                    }
                }
            }
        }
	}
}