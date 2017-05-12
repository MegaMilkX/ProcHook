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
    /*HMODULE hLib;*/
    LPVOID remoteString;
    LPVOID LoadLibPtr;
    
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
    
    HANDLE t = CreateRemoteThread(proc, NULL, 0, (LPTHREAD_START_ROUTINE)LoadLibPtr, (LPVOID)remoteString, 0, NULL);
    
    if(t == INVALID_HANDLE_VALUE)
    {
        printf("CreateRemoteThread failed");
        return FALSE;
    }
    
    CloseHandle(proc);
    return TRUE;
}

int main(int argc, char* argv[])
{
    DWORD targetThread;
    wchar_t buf[MAX_PATH];
    
    targetThread = GetTargetThread(L"notepad.exe");
    if(!targetThread)
        return 0;
    
    GetFullPathName(L"inject.dll", MAX_PATH, buf, NULL);
    wprintf(buf); printf("\n");
    
    if(!InjectDLL(targetThread, buf))
    {
        printf("Injection failed\n");
        
    }
    while(1);
    
    return 0;
}