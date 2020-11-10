#include "RfgUtil.h"
#include "Log.h"
#include "common/filesystem/Path.h"

namespace RfgUtil
{
    string CpuFilenameToGpuFilename(const string& cpuFilename)
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
        else
            THROW_EXCEPTION("Unknown rfg file extension '{}' passed to RfgUtil::CpuFilenameToGpuFilename()", extension);
    }
}