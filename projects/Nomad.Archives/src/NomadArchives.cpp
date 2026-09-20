//
// Created by troplo on 10/17/26.
//

#include "NomadArchives.hpp"
#include "version.h"

namespace Nomad::Archives {
    const char *ProjectInterface::GetVersion() { return PROJECT_VERSION; }

    const char *ProjectInterface::GetBuildDate() { return PROJECT_BUILD_DATE; }
} // namespace Nomad::Archives
