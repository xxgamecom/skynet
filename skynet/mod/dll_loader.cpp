#include "dll_loader.h"

namespace skynet {

dll_loader::~dll_loader()
{
    unload();
}

} 
