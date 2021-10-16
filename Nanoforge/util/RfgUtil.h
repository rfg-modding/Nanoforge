#pragma once
#include "common/Typedefs.h"

class Config;

namespace RfgUtil
{
    string CpuFilenameToGpuFilename(std::string_view cpuFilename);
    bool ValidateDataPath(std::string_view dataPath, std::string_view missingFileName, bool logResult = true);
    bool AutoDetectDataPath(Config* config);
}