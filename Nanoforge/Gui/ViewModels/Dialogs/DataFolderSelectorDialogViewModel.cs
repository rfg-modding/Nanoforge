using System.Collections.ObjectModel;
using System.IO;
using System.Runtime.InteropServices;
using System.Threading.Tasks;
using Avalonia.Controls;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using Microsoft.Extensions.DependencyInjection;
using Nanoforge.Editor;
using Nanoforge.FileSystem;
using Nanoforge.Services;

namespace Nanoforge.Gui.ViewModels.Dialogs;

public partial class DataFolderSelectorDialogViewModel : ViewModelBase
{
    [ObservableProperty]
    private string _dataFolder = "";

    [ObservableProperty]
    private ObservableCollection<string> _searchResults = new();

    [ObservableProperty]
    private ObservableCollection<string> _validationWarnings = new();

    [ObservableProperty]
    private string? _selectedInstall = "";

    [ObservableProperty]
    private bool _haveSearchResults = false;
    
    [ObservableProperty]
    private bool _haveValidationWarnings = false;

    [ObservableProperty]
    private bool _validDataPathSelected = false;

    public DataFolderSelectorDialogViewModel()
    {
        //Auto search for RFG installs and populate list of results
        AutoDetectRfg();

        if (GeneralSettings.CVar.Value.DataPath.Length > 0)
        {
            _dataFolder = GeneralSettings.CVar.Value.DataPath;
        }
        else
        {
            _dataFolder = _searchResults[0];
        }
        ValidateDataFolder();
    }

    partial void OnSelectedInstallChanged(string? value)
    {
        if (value != null && value.Length > 0)
        {
            DataFolder = value;
            ValidateDataFolder();
        }
    }

    [RelayCommand]
    public void ScanForRfgInstalls()
    {
        AutoDetectRfg();
        if (SearchResults.Count == 1)
        {
            DataFolder = SearchResults[0];
        }
    }

    [RelayCommand]
    private async Task BrowseToDataFolder()
    {
        IFileDialogService? fileDialog = App.Current.Services.GetService<IFileDialogService>();
        if (fileDialog != null)
        {
            var result = await fileDialog.ShowOpenFolderDialogAsync(this);
            if (result != null)
            {
                if (result.Count > 0)
                {
                    DataFolder = result[0].Path.AbsolutePath;
                }
            }
        }
    }

    [RelayCommand]
    public void ConfirmSelection(Window window)
    {
        GeneralSettings.CVar.Value.DataPath = DataFolder;
        GeneralSettings.CVar.Save();
        PackfileVFS.MountDataFolderAsync("//data/", GeneralSettings.CVar.Value.DataPath);
        window.Close();
    }

    [RelayCommand]
    public void CancelSelection(Window window)
    {
        window.Close();
    }

    private void ValidateDataFolder()
    {
        string[] expectedFiles =
        [
            "activities.vpp_pc", "anims.vpp_pc", "chunks.vpp_pc", "cloth_sim.vpp_pc", "decals.vpp_pc", "dlc01_l0.vpp_pc", "dlc01_l1.vpp_pc", "dlc01_precache.vpp_pc",
            "dlcp01_activities.vpp_pc", "dlcp01_anims.vpp_pc", "dlcp01_cloth_sim.vpp_pc", "dlcp01_effects.vpp_pc", "dlcp01_humans.vpp_pc", "dlcp01_interface.vpp_pc",
            "dlcp01_items.vpp_pc", "dlcp01_misc.vpp_pc", "dlcp01_missions.vpp_pc", "dlcp01_personas_en_us.vpp_pc", "dlcp01_vehicles_r.vpp_pc", "dlcp01_voices_en_us.vpp_pc",
            "dlcp02_interface.vpp_pc", "dlcp02_misc.vpp_pc", "dlcp03_interface.vpp_pc", "dlcp03_misc.vpp_pc", "effects.vpp_pc", "effects_mp.vpp_pc", "humans.vpp_pc",
            "interface.vpp_pc", "items.vpp_pc", "items_mp.vpp_pc", "misc.vpp_pc", "missions.vpp_pc", "mp_common.vpp_pc", "mp_cornered.vpp_pc", "mp_crashsite.vpp_pc",
            "mp_crescent.vpp_pc", "mp_crevice.vpp_pc", "mp_deadzone.vpp_pc", "mp_downfall.vpp_pc", "mp_excavation.vpp_pc", "mp_fallfactor.vpp_pc", "mp_framework.vpp_pc",
            "mp_garrison.vpp_pc", "mp_gauntlet.vpp_pc", "mp_overpass.vpp_pc", "mp_pcx_assembly.vpp_pc", "mp_pcx_crossover.vpp_pc", "mp_pinnacle.vpp_pc", "mp_quarantine.vpp_pc",
            "mp_radial.vpp_pc", "mp_rift.vpp_pc", "mp_sandpit.vpp_pc", "mp_settlement.vpp_pc", "mp_warlords.vpp_pc", "mp_wasteland.vpp_pc", "mp_wreckage.vpp_pc",
            "mpdlc_broadside.vpp_pc", "mpdlc_division.vpp_pc", "mpdlc_islands.vpp_pc", "mpdlc_landbridge.vpp_pc", "mpdlc_minibase.vpp_pc", "mpdlc_overhang.vpp_pc",
            "mpdlc_puncture.vpp_pc", "mpdlc_ruins.vpp_pc", "personas_de_de.vpp_pc", "personas_en_us.vpp_pc", "personas_es_es.vpp_pc", "personas_fr_fr.vpp_pc",
            "personas_it_it.vpp_pc", "personas_ru_ru.vpp_pc", "skybox.vpp_pc", "sounds_r.vpp_pc", "steam.vpp_pc",
            /*"table.vpp_pc", //Not checked since people often rename or delete this for misc.vpp_pc mods to work*/
            "terr01_l0.vpp_pc", "terr01_l1.vpp_pc", "terr01_precache.vpp_pc", "vehicles_r.vpp_pc", "voices_de_de.vpp_pc", "voices_en_us.vpp_pc", "voices_es_es.vpp_pc",
            "voices_fr_fr.vpp_pc", "voices_it_it.vpp_pc", "voices_ru_ru.vpp_pc", "wc1.vpp_pc", "wc10.vpp_pc", "wc2.vpp_pc", "wc3.vpp_pc", "wc4.vpp_pc", "wc5.vpp_pc", "wc6.vpp_pc",
            "wc7.vpp_pc", "wc8.vpp_pc", "wc9.vpp_pc", "wcdlc1.vpp_pc", "wcdlc2.vpp_pc", "wcdlc3.vpp_pc", "wcdlc4.vpp_pc", "wcdlc5.vpp_pc", "wcdlc6.vpp_pc",
            "wcdlc7.vpp_pc", "wcdlc8.vpp_pc", "wcdlc9.vpp_pc", "zonescript_dlc01.vpp_pc", "zonescript_terr01.vpp_pc"
        ];

        ValidationWarnings.Clear();
        foreach (string filename in expectedFiles)
        {
            string filePath = $"{DataFolder}{filename}";
            if (!File.Exists(filePath))
            {
                ValidationWarnings.Add($"{filename} is missing");
            }
        }
        HaveValidationWarnings = ValidationWarnings.Count > 0;
        ValidDataPathSelected = DataFolder.Trim().Length > 0 && Path.Exists(DataFolder);
    }

    private void AutoDetectRfg()
    {
        string[] searchTemplates =
        [
            "Steam/steamapps/common/Red Faction Guerrilla Re-MARS-tered/data/",
            "SteamLibrary/steamapps/common/Red Faction Guerrilla Re-MARS-tered/data/",
            "GOG/Games/Red Faction Guerrilla Re-Mars-tered/data/",
            "Program Files (x86)/Steam/steamapps/common/Red Faction Guerrilla Re-MARS-tered/data/",
            "Program Files (x86)/SteamLibrary/steamapps/common/Red Faction Guerrilla Re-MARS-tered/data/",
            "Program Files (x86)/GOG/Games/Red Faction Guerrilla Re-MARS-tered/data/",
            "Program Files/Steam/steamapps/common/Red Faction Guerrilla Re-MARS-tered/data/",
            "Program Files/SteamLibrary/steamapps/common/Red Faction Guerrilla Re-MARS-tered/data/",
            "Program Files/GOG/Games/Red Faction Guerrilla Re-MARS-tered/data/",
            "Games/SteamLibrary/steamapps/common/Red Faction Guerrilla Re-MARS-tered/data/",
            "Games/GOG Games/Red Faction Guerrilla Re-MARS-tered/data",
            "Games/GOG/Red Faction Guerrilla Re-MARS-tered/data"
        ];

        SearchResults.Clear();
        if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
        {
            const string alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
            foreach (char driveLetter in alphabet)
            {
                foreach (string template in searchTemplates)
                {
                    string searchPath = $"{driveLetter}:/{template}";
                    if (Directory.Exists(searchPath))
                    {
                        SearchResults.Add(searchPath);
                    }
                }
            }
        }
        else if (RuntimeInformation.IsOSPlatform(OSPlatform.Linux))
        {
            if (Directory.Exists("/mnt"))
            {
                foreach (var directory in Directory.GetDirectories("/mnt"))
                {
                    foreach (string template in searchTemplates)
                    {
                        //TODO: See if adding the / is really necessary
                        string searchPath = $"{directory}/{template}";
                        if (Directory.Exists(searchPath))
                        {
                            SearchResults.Add(searchPath);
                        }
                    }
                }
            }
        }
        HaveSearchResults = SearchResults.Count > 0;
    }
}