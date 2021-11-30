#pragma once
#include "common/Typedefs.h"
#include <optional>
#include <vector>

class PackfileVFS;
class Config;

//Todo: Support locales using non-latin alphabets
//Note:
//  - Only the locales with an rfglocatext file included with the vanilla game are listed currently.
//  - Only locales using the Latin alphabet are supported currently. The others are commented out.
enum class Locale
{
    //Using Arabic alphabet
    //AR_EG, //Arabic (Egypt)

    //Using Latin alphabet
    CS_CZ, //Czech (Czech_Republic)
    DE_DE, //German (Germany)
    EN_US, //English (United States)
    ES_ES, //Spanish (Spain)
    //ES_MX, //Spanish (Mexico) //Note: Not yet supported by vanilla game. Rfglocatext file is filled with english strings.
    FR_FR, //French (France)
    IT_IT, //Italian (Italy)
    //PT_BR, //Portuguese (Brazil) //Note: Not yet supported by vanilla game. Rfglocatext file is filled with english strings.

    //Using Polish alphabet
    PL_PL, //Polish (Poland) //Note: Some characters of the Polish alphabet are not supported by the font Nanoforge uses.

    //Using Japanese alphabet
    //JA_JP, //Japanese (Japan)

    //Using Korean alphabet
    //KO_KR, //Korean (Korea)

    //Using Cyrillic alphabet
    //RU_RU, //Russian (Russia)

    //Using Chinese alphabet
    //ZH_CN, //Chinese (China)

    None
};

struct LocalizedString
{
    u32 KeyHash;
    string String;
};

struct LocalizationClass
{
    Locale Type;
    string Name;
    std::vector<LocalizedString> Strings = {};
};

//Manages RFG string localizations stored in .rfglocatext files in misc.vpp_pc
class Localization
{
public:
    void Init(PackfileVFS* packfileVFS);
    void LoadLocalizationData();

    LocalizationClass* GetLocale(Locale locale);
    string GetLocaleName(Locale locale);
    //Attempt to get a localized string using it's key
    std::optional<string> StringFromKey(std::string_view key);
    bool Ready() { return ready_; }

    Locale CurrentLocale = Locale::None;
    std::vector<LocalizationClass> Classes = {};

private:
    void LoadLocalizationClass(std::string_view filename, std::string_view className, Locale locale);

    PackfileVFS* packfileVFS_ = nullptr;
    bool ready_ = false;
};

//Convert std::string to Locale enum
#pragma warning(disable:4505)
static Locale ParseLocaleString(std::string_view value)
{
    //if (value == "AR_EG")
    //    return Locale::AR_EG;
    if (value == "CS_CZ")
        return Locale::CS_CZ;
    if (value == "DE_DE")
        return Locale::DE_DE;
    if (value == "EN_US")
        return Locale::EN_US;
    if (value == "ES_ES")
        return Locale::ES_ES;
    //if (value == "ES_MX")
    //    return Locale::ES_MX;
    if (value == "FR_FR")
        return Locale::FR_FR;
    if (value == "IT_IT")
        return Locale::IT_IT;
    //if (value == "PT_BR")
    //    return Locale::PT_BR;
    if (value == "PL_PL")
        return Locale::PL_PL;
    //if (value == "JA_JP")
    //    return Locale::JA_JP;
    //if (value == "KO_KR")
    //    return Locale::KO_KR;
    //if (value == "RU_RU")
    //    return Locale::RU_RU;
    //if (value == "ZH_CN")
    //    return Locale::ZH_CN;

    return Locale::None;
}
#pragma warning(default:4505)