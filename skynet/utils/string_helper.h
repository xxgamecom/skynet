#pragma once

namespace skynet {

class string_helper final
{
public:
    // duplicate string (need release)
    static char* dup(const char* str);
};

}
