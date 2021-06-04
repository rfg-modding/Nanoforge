#pragma once
#include "common/Typedefs.h"

class Config;

namespace RfgUtil
{
    string CpuFilenameToGpuFilename(const string& cpuFilename);
    bool ValidateDataPath(const string& dataPath, string& missingFileName, bool logResult = true);
    bool AutoDetectDataPath(Config* config);
}