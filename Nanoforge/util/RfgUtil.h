#pragma once
#include "common/Typedefs.h"

namespace RfgUtil
{
    string CpuFilenameToGpuFilename(const string& cpuFilename);
    bool ValidateDataPath(const string& dataPath, string& errorMessage);
}