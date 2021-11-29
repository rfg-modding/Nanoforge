#include "Localization.h"
#include "Log.h"
#include "RfgTools++/hashes/Hash.h"
#include "rfg/PackfileVFS.h"
#include "application/Config.h"
#include <RfgTools++/formats/localization/LocalizationFile3.h>
#include <RfgTools++/formats/packfiles/Packfile3.h>
#include <locale>
#include <codecvt>

void Localization::Init(PackfileVFS* packfileVFS, Config* config)
{
    TRACE();
    packfileVFS_ = packfileVFS;

    //Get default locale from config
    if (!config->Exists("DefaultLocale"))
        config->CreateVariable("DefaultLocale", ConfigType::String, "EN_US", "Default locale to use when viewing localized RFG strings. Nanoforge itself doesn't support localization yet.");

    //Parse default locale
    auto var = config->GetVariable("DefaultLocale");
    var->ShownInSettings = false;
    CurrentLocale = ParseLocaleString(std::get<string>(var->Value));
    if (CurrentLocale == Locale::None)
        CurrentLocale = Locale::EN_US;

    config->Save();
}

void Localization::LoadLocalizationData()
{
    TRACE();
    //Load all supported rfglocatext files
    //LoadLocalizationClass("locatexts_ar_eg.rfglocatext", "Arabic (Egypt)", Locale::AR_EG);
    LoadLocalizationClass("locatexts_cs_cz.rfglocatext", "Czech (Czech Republic)", Locale::CS_CZ);
    LoadLocalizationClass("locatexts_de_de.rfglocatext", "German (Germany)", Locale::DE_DE);
    LoadLocalizationClass("locatexts_en_us.rfglocatext", "English (United States)", Locale::EN_US);
    LoadLocalizationClass("locatexts_es_es.rfglocatext", "Spanish (Spain)", Locale::ES_ES);
    //LoadLocalizationClass("locatexts_es_mx.rfglocatext", "Spanish (Mexico)", Locale::ES_MX);
    LoadLocalizationClass("locatexts_fr_fr.rfglocatext", "French (France)", Locale::FR_FR);
    LoadLocalizationClass("locatexts_it_it.rfglocatext", "Italian (Italy)", Locale::IT_IT);
    //LoadLocalizationClass("locatexts_ja_jp.rfglocatext", "Japanese (Japan)", Locale::JA_JP);
    //LoadLocalizationClass("locatexts_ko_kr.rfglocatext", "Korean (Korea)", Locale::KO_KR);
    LoadLocalizationClass("locatexts_pl_pl.rfglocatext", "Polish (Poland)", Locale::PL_PL);
    //LoadLocalizationClass("locatexts_pt_br.rfglocatext", "Portuguese (Brazil)", Locale::PT_BR);
    //LoadLocalizationClass("locatexts_ru_ru.rfglocatext", "Russian (Russia)", Locale::RU_RU);
    //LoadLocalizationClass("locatexts_zh_cn.rfglocatext", "Chinese (China)", Locale::ZH_CN);

    ready_ = true;
}

LocalizationClass* Localization::GetLocale(Locale locale)
{
    for (auto& localeClass : Classes)
        if (localeClass.Type == locale)
            return &localeClass;

    return nullptr;
}

string Localization::GetLocaleName(Locale locale)
{
    for (auto& localeClass : Classes)
        if (localeClass.Type == locale)
            return localeClass.Name;

    return "Unknown Locale";
}

std::optional<string> Localization::StringFromKey(std::string_view key)
{
    //Hash key and get current locale
    u32 keyHash = Hash::HashVolitionCRCAlt(key, 0);
    LocalizationClass* localeClass = GetLocale(CurrentLocale);
    if (!localeClass)
        return {};

    //Search for string with matching key hash
    for (auto& str : localeClass->Strings)
        if (str.KeyHash == keyHash)
            return str.String;

    return {};
}

void Localization::LoadLocalizationClass(std::string_view filename, std::string_view className, Locale locale)
{
    //Get packfile. All rfglocatext files are in misc.vpp_pc so we don't need to check if it's in str2_pc
    Handle<Packfile3> vpp = packfileVFS_->GetPackfile("misc.vpp_pc");
    if (!vpp)
    {
        LOG_ERROR("Failed to get misc.vpp_pc in Localization.cpp.", filename);
        return;
    }

    //Extract rfglocatext bytes
    std::optional<std::vector<u8>> fileBytes = vpp->ExtractSingleFile(filename, true);
    if (!fileBytes)
    {
        LOG_ERROR("Failed to extract {} in Localization.cpp.", filename);
        return;
    }

    //Parse rfglocatext file
    LocalizationFile3 localizationFile;
    BinaryReader reader(fileBytes.value());
    localizationFile.Read(reader, filename);

    //Create locale class
    auto& localeClass = Classes.emplace_back();
    localeClass.Type = locale;
    localeClass.Name = className;

#pragma warning(disable:4996) //Disable warning about <codecvt> being deprecated. Fine for now since MSVC still lets us use it in c++20 and there's no standard replacement.
    //Convert strings from std::wstring to std::string and cache them in the locale class
    using convert_type = std::codecvt_utf8<wchar_t>;
    std::wstring_convert<convert_type, wchar_t> converter;
    for (u32 i = 0; i < localizationFile.NumStrings; i++)
    {
        auto& localizedString = localeClass.Strings.emplace_back();
        localizedString.KeyHash = localizationFile.Entries[i].KeyHash;
        localizedString.String = converter.to_bytes(localizationFile.EntryData[i]);
    }
#pragma warning(default:4996)
}