#include "inject.h"

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <io.h>
#include <fcntl.h>

#include <d3d11.h>

#pragma comment(lib, "d3d11.lib")

#define PRINT(MSG) fprintf(hf_out, MSG); fprintf(hf_out, "\n")
#define WPRINT(MSG) fwprintf(hf_out, MSG); fwprintf(hf_out, L"\n")

FILE* hf_out;
FILE* hf_in;
void InitConsoleWindow()
{
    AllocConsole();
    
    HANDLE handle_out = GetStdHandle(STD_OUTPUT_HANDLE);
    int hCrt = _open_osfhandle((__int64)handle_out, _O_TEXT);
    hf_out = _fdopen(hCrt, "w");
    setvbuf(hf_out, NULL, _IONBF, 1);
    
    HANDLE handle_in = GetStdHandle(STD_INPUT_HANDLE);
    hCrt = _open_osfhandle((__int64)handle_in, _O_TEXT);
    hf_in = _fdopen(hCrt, "r");
    setvbuf(hf_in, NULL, _IONBF, 128);
}

DLLEXPORT void Init(void)
{
    InitConsoleWindow();
    wchar_t moduleName[MAX_PATH];
    GetModuleFileName(NULL, moduleName, sizeof(wchar_t) * MAX_PATH);
    WPRINT(moduleName);
    PRINT("Hello");
    
    
}


BOOL APIENTRY DllMain(HINSTANCE hInst, DWORD reason, LPVOID reserved)
{
    switch(reason)
    {
    case DLL_PROCESS_ATTACH:
        break;
    case DLL_PROCESS_DETACH:
        break;
    case DLL_THREAD_ATTACH:
        break;
    case DLL_THREAD_DETACH:
        break;
    }
    
    return TRUE;
}