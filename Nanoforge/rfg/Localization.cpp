#include "Localization.h"
#include "Log.h"
#include "RfgTools++/hashes/Hash.h"
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

std::optional<string> Localization::StringFromKey(const string& key)
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

void Localization::LoadLocalizationClass(const string& filename, const string& className, Locale locale)
{
    //Get packfile. All rfglocatext files are in misc.vpp_pc so we don't need to check if it's in str2_pc
    Packfile3* container = packfileVFS_->GetPackfile("misc.vpp_pc");
    if (!container)
    {
        Log->error("Failed to get misc.vpp_pc in Localization.cpp.", filename);
        return;
    }

    //Extract rfglocatext bytes
    std::optional<std::span<u8>> fileBytes = container->ExtractSingleFile(filename, true);
    if (!fileBytes)
    {
        Log->error("Failed to extract {} in Localization.cpp.", filename);
        return;
    }

    //Parse rfglocatext file
    LocalizationFile3 localizationFile;
    BinaryReader reader(fileBytes.value());
    localizationFile.Read(reader, filename);

    //Free byte buffer
    delete[] fileBytes.value().data();

    //Create locale class
    auto& localeClass = Classes.emplace_back();
    localeClass.Type = locale;
    localeClass.Name = className;

    //Convert strings from std::wstring to std::string and cache them in the locale class
    using convert_type = std::codecvt_utf8<wchar_t>;
    std::wstring_convert<convert_type, wchar_t> converter;
    for (u32 i = 0; i < localizationFile.NumStrings; i++)
    {
        auto& localizedString = localeClass.Strings.emplace_back();
        localizedString.KeyHash = localizationFile.Entries[i].KeyHash;
        localizedString.String = converter.to_bytes(localizationFile.EntryData[i]);
    }
}