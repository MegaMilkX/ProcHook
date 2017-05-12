#include "inject.h"

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

DLLEXPORT void Foo(void)
{
    Beep(3000, 500);
    ExitProcess(0);
}

BOOL APIENTRY DllMain(HINSTANCE hInst, DWORD reason, LPVOID reserved)
{
    switch(reason)
    {
    case DLL_PROCESS_ATTACH:
        Foo();
        break;
    case DLL_PROCESS_DETACH:
        break;
    case DLL_THREAD_ATTACH:
        Foo();
        break;
    case DLL_THREAD_DETACH:
        break;
    }
    
    return TRUE;
}