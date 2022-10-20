#ifndef VERSION_H
#define VERSION_H
#include <string.h>

#define MAJOR_VERSION 0
#define MINOR_VERSION 6
#define PATCH_VERSION 29

namespace xcite {
inline std::string get_version()
{
  return std::to_string(MAJOR_VERSION)+"."+std::to_string(MINOR_VERSION)+"."+std::to_string(PATCH_VERSION);
}

}


#endif //VERSION_H
