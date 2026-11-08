#ifdef _WIN32
#include <windows.h>

extern "C"
BOOL WINAPI DllMain( HINSTANCE hInstance, DWORD dwReason, LPVOID )
{
    return 1;
}
#else
// Linux has no DllMain, so a library constructor installs the tables the .rc parser generated
#include "dynres.h"

void __attribute__((constructor)) InitializeResourceSystem() {
    setup_string_tables();
    setup_cursors();
    return;
}

#endif
