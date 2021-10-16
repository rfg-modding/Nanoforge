#include "RfgUtil.h"
#include "Log.h"
#include "common/filesystem/Path.h"
#include "application/Config.h"

namespace RfgUtil
{
    string CpuFilenameToGpuFilename(std::string_view cpuFilename)
    {
        //Todo: Support more extensions
        string extension = Path::GetExtension(cpuFilename);
        string filenameNoExt = Path::GetFileNameNoExtension(cpuFilename);
        if (extension == ".cpeg_pc")
            return filenameNoExt + ".gpeg_pc";
        else if (extension == ".cvbm_pc")
            return filenameNoExt + ".gvbm_pc";
        else if (extension == ".csmesh_pc")
            return filenameNoExt + ".gsmesh_pc";
        else if (extension == ".ccmesh_pc")
            return filenameNoExt + ".gcmesh_pc";
        else if (extension == ".cchk_pc")
            return filenameNoExt + ".gchk_pc";
        else
        {
            LOG_ERROR("Unknown rfg file extension \"{}\"", extension);
            return string(cpuFilename);
        }
    }

    bool ValidateDataPath(std::string_view dataPath, std::string_view missingFileName, bool logResult)
    {
        //List of expected vpp_pc files
        std::vector<string> expectedFiles =
        {
            "activities.vpp_pc",
            "anims.vpp_pc",
            "chunks.vpp_pc",
            "cloth_sim.vpp_pc",
            "decals.vpp_pc",
            "dlc01_l0.vpp_pc",
            "dlc01_l1.vpp_pc",
            "dlc01_precache.vpp_pc",
            "dlcp01_activities.vpp_pc",
            "dlcp01_anims.vpp_pc",
            "dlcp01_cloth_sim.vpp_pc",
            "dlcp01_effects.vpp_pc",
            "dlcp01_humans.vpp_pc",
            "dlcp01_interface.vpp_pc",
            "dlcp01_items.vpp_pc",
            "dlcp01_misc.vpp_pc",
            "dlcp01_missions.vpp_pc",
            "dlcp01_personas_en_us.vpp_pc",
            "dlcp01_vehicles_r.vpp_pc",
            "dlcp01_voices_en_us.vpp_pc",
            "dlcp02_interface.vpp_pc",
            "dlcp02_misc.vpp_pc",
            "dlcp03_interface.vpp_pc",
            "dlcp03_misc.vpp_pc",
            "effects.vpp_pc",
            "effects_mp.vpp_pc",
            "humans.vpp_pc",
            "interface.vpp_pc",
            "items.vpp_pc",
            "items_mp.vpp_pc",
            "misc.vpp_pc",
            "missions.vpp_pc",
            "mp_common.vpp_pc",
            "mp_cornered.vpp_pc",
            "mp_crashsite.vpp_pc",
            "mp_crescent.vpp_pc",
            "mp_crevice.vpp_pc",
            "mp_deadzone.vpp_pc",
            "mp_downfall.vpp_pc",
            "mp_excavation.vpp_pc",
            "mp_fallfactor.vpp_pc",
            "mp_framework.vpp_pc",
            "mp_garrison.vpp_pc",
            "mp_gauntlet.vpp_pc",
            "mp_overpass.vpp_pc",
            "mp_pcx_assembly.vpp_pc",
            "mp_pcx_crossover.vpp_pc",
            "mp_pinnacle.vpp_pc",
            "mp_quarantine.vpp_pc",
            "mp_radial.vpp_pc",
            "mp_rift.vpp_pc",
            "mp_sandpit.vpp_pc",
            "mp_settlement.vpp_pc",
            "mp_warlords.vpp_pc",
            "mp_wasteland.vpp_pc",
            "mp_wreckage.vpp_pc",
            "mpdlc_broadside.vpp_pc",
            "mpdlc_division.vpp_pc",
            "mpdlc_islands.vpp_pc",
            "mpdlc_landbridge.vpp_pc",
            "mpdlc_minibase.vpp_pc",
            "mpdlc_overhang.vpp_pc",
            "mpdlc_puncture.vpp_pc",
            "mpdlc_ruins.vpp_pc",
            "personas_de_de.vpp_pc",
            "personas_en_us.vpp_pc",
            "personas_es_es.vpp_pc",
            "personas_fr_fr.vpp_pc",
            "personas_it_it.vpp_pc",
            "personas_ru_ru.vpp_pc",
            "skybox.vpp_pc",
            "sounds_r.vpp_pc",
            "steam.vpp_pc",
            //"table.vpp_pc", //Not checked since it's required to rename or delete this for misc.vpp_pc mods to work
            "terr01_l0.vpp_pc",
            "terr01_l1.vpp_pc",
            "terr01_precache.vpp_pc",
            "vehicles_r.vpp_pc",
            "voices_de_de.vpp_pc",
            "voices_en_us.vpp_pc",
            "voices_es_es.vpp_pc",
            "voices_fr_fr.vpp_pc",
            "voices_it_it.vpp_pc",
            "voices_ru_ru.vpp_pc",
            "wc1.vpp_pc",
            "wc10.vpp_pc",
            "wc2.vpp_pc",
            "wc3.vpp_pc",
            "wc4.vpp_pc",
            "wc5.vpp_pc",
            "wc6.vpp_pc",
            "wc7.vpp_pc",
            "wc8.vpp_pc",
            "wc9.vpp_pc",
            "wcdlc1.vpp_pc",
            "wcdlc2.vpp_pc",
            "wcdlc3.vpp_pc",
            "wcdlc4.vpp_pc",
            "wcdlc5.vpp_pc",
            "wcdlc6.vpp_pc",
            "wcdlc7.vpp_pc",
            "wcdlc8.vpp_pc",
            "wcdlc9.vpp_pc",
            "zonescript_dlc01.vpp_pc",
            "zonescript_terr01.vpp_pc"
        };

        //Check if all expected files are in the data folder. Return false and set missing file string
        for (auto& filename : expectedFiles)
        {
            if (!std::filesystem::exists(fmt::format("{}\\{}", dataPath, filename)))
            {
                if(logResult)
                    Log->warn("Current data path is invalid. Data path: \"{}\", first missing vpp: \"{}\"", dataPath, filename);

                missingFileName = filename;
                return false;
            }
        }

        if(logResult)
            Log->info("Current data path is valid. Data path: \"{}\"", dataPath);

        return true;
    }

    //Todo: Add non re-mars-tered paths
    //Non exhaustive list of common/expected install locations
    const std::vector<string> CommonInstallLocations =
    {
        "A:/Steam/steamapps/common/Red Faction Guerrilla Re-MARS-tered/data",
        "A:/SteamLibrary/steamapps/common/Red Faction Guerrilla Re-MARS-tered/data",
        "A:/GOG/Games/Red Faction Guerrilla Re-MARS-tered/data",
        "A:/Program Files (x86)/Steam/steamapps/common/Red Faction Guerrilla Re-MARS-tered/data",
        "A:/Program Files (x86)/SteamLibrary/steamapps/common/Red Faction Guerrilla Re-MARS-tered/data",
        "A:/Program Files (x86)/GOG/Games/Red Faction Guerrilla Re-MARS-tered/data",
        "A:/Program Files/Steam/steamapps/common/Red Faction Guerrilla Re-MARS-tered/data",
        "A:/Program Files/SteamLibrary/steamapps/common/Red Faction Guerrilla Re-MARS-tered/data",
        "A:/Program Files/GOG/Games/Red Faction Guerrilla Re-MARS-tered/data",

        "B:/Steam/steamapps/common/Red Faction Guerrilla Re-MARS-tered/data",
        "B:/SteamLibrary/steamapps/common/Red Faction Guerrilla Re-MARS-tered/data",
        "B:/GOG/Games/Red Faction Guerrilla Re-MARS-tered/data",
        "B:/Program Files (x86)/Steam/steamapps/common/Red Faction Guerrilla Re-MARS-tered/data",
        "B:/Program Files (x86)/SteamLibrary/steamapps/common/Red Faction Guerrilla Re-MARS-tered/data",
        "B:/Program Files (x86)/GOG/Games/Red Faction Guerrilla Re-MARS-tered/data",
        "B:/Program Files/Steam/steamapps/common/Red Faction Guerrilla Re-MARS-tered/data",
        "B:/Program Files/SteamLibrary/steamapps/common/Red Faction Guerrilla Re-MARS-tered/data",
        "B:/Program Files/GOG/Games/Red Faction Guerrilla Re-MARS-tered/data",

        "C:/Steam/steamapps/common/Red Faction Guerrilla Re-MARS-tered/data",
        "C:/SteamLibrary/steamapps/common/Red Faction Guerrilla Re-MARS-tered/data",
        "C:/GOG/Games/Red Faction Guerrilla Re-MARS-tered/data",
        "C:/Program Files (x86)/Steam/steamapps/common/Red Faction Guerrilla Re-MARS-tered/data",
        "C:/Program Files (x86)/SteamLibrary/steamapps/common/Red Faction Guerrilla Re-MARS-tered/data",
        "C:/Program Files (x86)/GOG/Games/Red Faction Guerrilla Re-MARS-tered/data",
        "C:/Program Files/Steam/steamapps/common/Red Faction Guerrilla Re-MARS-tered/data",
        "C:/Program Files/SteamLibrary/steamapps/common/Red Faction Guerrilla Re-MARS-tered/data",
        "C:/Program Files/GOG/Games/Red Faction Guerrilla Re-MARS-tered/data",

        "D:/Steam/steamapps/common/Red Faction Guerrilla Re-MARS-tered/data",
        "D:/SteamLibrary/steamapps/common/Red Faction Guerrilla Re-MARS-tered/data",
        "D:/GOG/Games/Red Faction Guerrilla Re-MARS-tered/data",
        "D:/Program Files (x86)/Steam/steamapps/common/Red Faction Guerrilla Re-MARS-tered/data",
        "D:/Program Files (x86)/SteamLibrary/steamapps/common/Red Faction Guerrilla Re-MARS-tered/data",
        "D:/Program Files (x86)/GOG/Games/Red Faction Guerrilla Re-MARS-tered/data",
        "D:/Program Files/Steam/steamapps/common/Red Faction Guerrilla Re-MARS-tered/data",
        "D:/Program Files/SteamLibrary/steamapps/common/Red Faction Guerrilla Re-MARS-tered/data",
        "D:/Program Files/GOG/Games/Red Faction Guerrilla Re-MARS-tered/data",

        "E:/Steam/steamapps/common/Red Faction Guerrilla Re-MARS-tered/data",
        "E:/SteamLibrary/steamapps/common/Red Faction Guerrilla Re-MARS-tered/data",
        "E:/GOG/Games/Red Faction Guerrilla Re-MARS-tered/data",
        "E:/Program Files (x86)/Steam/steamapps/common/Red Faction Guerrilla Re-MARS-tered/data",
        "E:/Program Files (x86)/SteamLibrary/steamapps/common/Red Faction Guerrilla Re-MARS-tered/data",
        "E:/Program Files (x86)/GOG/Games/Red Faction Guerrilla Re-MARS-tered/data",
        "E:/Program Files/Steam/steamapps/common/Red Faction Guerrilla Re-MARS-tered/data",
        "E:/Program Files/SteamLibrary/steamapps/common/Red Faction Guerrilla Re-MARS-tered/data",
        "E:/Program Files/GOG/Games/Red Faction Guerrilla Re-MARS-tered/data",

        "F:/Steam/steamapps/common/Red Faction Guerrilla Re-MARS-tered/data",
        "F:/SteamLibrary/steamapps/common/Red Faction Guerrilla Re-MARS-tered/data",
        "F:/GOG/Games/Red Faction Guerrilla Re-MARS-tered/data",
        "F:/Program Files (x86)/Steam/steamapps/common/Red Faction Guerrilla Re-MARS-tered/data",
        "F:/Program Files (x86)/SteamLibrary/steamapps/common/Red Faction Guerrilla Re-MARS-tered/data",
        "F:/Program Files (x86)/GOG/Games/Red Faction Guerrilla Re-MARS-tered/data",
        "F:/Program Files/Steam/steamapps/common/Red Faction Guerrilla Re-MARS-tered/data",
        "F:/Program Files/SteamLibrary/steamapps/common/Red Faction Guerrilla Re-MARS-tered/data",
        "F:/Program Files/GOG/Games/Red Faction Guerrilla Re-MARS-tered/data",

        "G:/Steam/steamapps/common/Red Faction Guerrilla Re-MARS-tered/data",
        "G:/SteamLibrary/steamapps/common/Red Faction Guerrilla Re-MARS-tered/data",
        "G:/GOG/Games/Red Faction Guerrilla Re-MARS-tered/data",
        "G:/Program Files (x86)/Steam/steamapps/common/Red Faction Guerrilla Re-MARS-tered/data",
        "G:/Program Files (x86)/SteamLibrary/steamapps/common/Red Faction Guerrilla Re-MARS-tered/data",
        "G:/Program Files (x86)/GOG/Games/Red Faction Guerrilla Re-MARS-tered/data",
        "G:/Program Files/Steam/steamapps/common/Red Faction Guerrilla Re-MARS-tered/data",
        "G:/Program Files/SteamLibrary/steamapps/common/Red Faction Guerrilla Re-MARS-tered/data",
        "G:/Program Files/GOG/Games/Red Faction Guerrilla Re-MARS-tered/data",
    };

    //Attempt to auto locate the data path from a list of common install locations. Will set the "Data path" config var if it's found.
    bool AutoDetectDataPath(Config* config)
    {
        //Get data path config var
        config->EnsureVariableExists("Data path", ConfigType::String);
        auto dataPathVar = config->GetVariable("Data path");
        dataPathVar->IsFolderPath = true;
        dataPathVar->IsFilePath = false;

        Log->info("Data path \"{}\" is invalid. Attempting to auto detect RFG install location.", std::get<string>(dataPathVar->Value));

        //Loop through all common install locations
        for (auto& path : CommonInstallLocations)
        {
            //If install location has all expected files set the data path to that
            string missingFileName;
            if (RfgUtil::ValidateDataPath(path, missingFileName, false))
            {
                Log->info("Auto detected RFG install location at \"{}\"", path);
                dataPathVar->Value = path;
                config->Save();
                return true;
            }
        }

        Log->warn("Failed to auto detect RFG install location.");
        return false;
    }
}