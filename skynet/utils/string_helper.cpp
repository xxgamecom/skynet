#include "string_helper.h"
#include "../memory/skynet_malloc.h"

#include <string>

namespace skynet {

char* string_helper::dup(const char* str)
{
    size_t sz = ::strlen(str);
    char* ret = (char*) skynet_malloc(sz + 1);
    ::memcpy(ret, str, sz + 1);
    return ret;
}

}

