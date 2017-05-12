#include <windows.h>
#include <tlhelp32.h> 
#include <shlwapi.h> 
#include <conio.h> 
#include <stdio.h> 

DWORD GetTargetThread(const wchar_t* procName)
{
    PROCESSENTRY32 pe;
    HANDLE thSnapshot;
    BOOL retval;
    
    thSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if(thSnapshot == INVALID_HANDLE_VALUE)
    {
        printf("thSnapshot Invalid handle value");
        return 0;
    }
    
    pe.dwSize = sizeof(PROCESSENTRY32);
    retval = Process32First(thSnapshot, &pe);
    while(retval)
    {
        wprintf(pe.szExeFile);
        printf("\n");
        if(StrStrI(pe.szExeFile, procName))
        {
            return pe.th32ProcessID;
        }
        retval = Process32Next(thSnapshot, &pe);
    }
    return 0;
}

BOOL InjectDLL(DWORD pId, const wchar_t* dllName)
{
    HANDLE proc;
    HANDLE hLib;
    LPVOID remoteString;
    LPVOID LoadLibPtr;
    LPVOID initPtr;
    HMODULE injectModule;
    
    if(!pId)
        return FALSE;
    
    proc = 
        OpenProcess(
            PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION, 
            FALSE, 
            pId
        );
    if(!proc)
    {
        printf("OpenProcess failed");
        return FALSE;
    }
    
    LoadLibPtr = (LPVOID)GetProcAddress(GetModuleHandle(L"kernel32.dll"), "LoadLibraryW");
    
    if(LoadLibPtr == NULL)
    {
        printf("GetProcAddress failed");
        return FALSE;
    }
    
    remoteString = 
        (LPVOID)VirtualAllocEx(
            proc, 
            NULL, 
            wcslen(dllName) * sizeof(wchar_t) + 1, 
            MEM_RESERVE | MEM_COMMIT, 
            PAGE_READWRITE
        );
        
    if(remoteString == NULL)
    {
        printf("VirtualAllocEx failed");
        return FALSE;
    }
    
    if(WriteProcessMemory(proc, (PBYTE)remoteString, dllName, wcslen(dllName) * sizeof(wchar_t) + 1, NULL) == FALSE)
    {
        printf("WriteProcessMemory failed");
        return FALSE;
    }
    
    // Return value of LoadLibraryW is truncated
    hLib = 
        (HANDLE)CreateRemoteThread(
            proc, 
            NULL, 
            0, 
            (LPTHREAD_START_ROUTINE)LoadLibPtr, 
            (LPVOID)remoteString, 
            0, 
            NULL
        );
    
    if(hLib == INVALID_HANDLE_VALUE)
    {
        printf("CreateRemoteThread failed");
        return FALSE;
    }
    
    injectModule = LoadLibraryW(L"inject.dll");
    initPtr = (LPVOID)GetProcAddress(GetModuleHandle(L"inject.dll"), "Init");
    if(!initPtr)
    {
        printf("GetProcAddress for Init() failed");
        FreeLibrary(injectModule);
        return FALSE;
    }
    __int64 relInitPtr = (LPBYTE)initPtr - (LPBYTE)injectModule;
    printf("Relative Init() ptr: %I64i", relInitPtr);
    //initPtr = (LPVOID)((LPBYTE)hLib + relInitPtr);
    FreeLibrary(injectModule);
    
    (HMODULE)CreateRemoteThread(
        proc, 
        NULL, 
        0, 
        (LPTHREAD_START_ROUTINE)initPtr, 
        NULL, 
        0, 
        NULL
    );
    
    CloseHandle(proc);
    return TRUE;
}

int main(int argc, char* argv[])
{
    DWORD targetThread;
    wchar_t buf[MAX_PATH];
    
    targetThread = GetTargetThread(L"Prey.exe");
    if(!targetThread)
    {
        printf("Target thread not found");
        while(1);
    }
    
    GetFullPathName(L"inject.dll", MAX_PATH, buf, NULL);
    wprintf(buf); printf("\n");
    
    if(!InjectDLL(targetThread, buf))
    {
        printf("Injection failed\n");
        
    }
    while(1);
    
    return 0;
}