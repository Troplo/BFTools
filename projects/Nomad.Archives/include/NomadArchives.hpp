//
// Created by troplo on 10/17/26.
//

#ifndef NOMADARCHIVES_NOMADARCHIVES_H
#define NOMADARCHIVES_NOMADARCHIVES_H

namespace Nomad::Archives {
    class ProjectInterface {
      public:
        static const char* GetVersion();
        static const char* GetBuildDate();
    };
} // namespace Nomad::Archives

#endif // NOMADARCHIVES_NOMADARCHIVES_H
