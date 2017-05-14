#include "inject.h"

#include <windows.h>
#include <TlHelp32.h>
#include <stdio.h>
#include <stdlib.h>
#include <io.h>
#include <fcntl.h>

#include <iostream>

#include <MinHook.h>

#include <map>

#include <d3d11.h>

#pragma comment(lib, "d3d11.lib")

#include "vtable-indices.h"

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

typedef HRESULT(__stdcall* D3D11PresentHook)(IDXGISwapChain* swapChain, UINT SyncInterval, UINT Flags);
D3D11PresentHook phookD3D11Present = NULL;

typedef void(__stdcall* VSSetShaderHook)(
    ID3D11DeviceContext* context,
    ID3D11VertexShader* vertexShader,
    ID3D11ClassInstance* const *classInstances,
    UINT nClassInstances);
VSSetShaderHook phookD3D11DeviceContextVSSetShader = NULL;

typedef void(__stdcall* PSSetShaderHook)(
    ID3D11DeviceContext* context,
    ID3D11PixelShader* pixelShader,
    ID3D11ClassInstance* const *classInstances,
    UINT nClassInstances);
PSSetShaderHook phookD3D11DeviceContextPSSetShader = NULL;

std::map<void*, bool> shaderDisableMap;
void* currentVertexShader;
void* currentPixelShader;

HRESULT __stdcall OnD3D11Present(IDXGISwapChain* swapChain, UINT interval, UINT flags)
{
    
    return phookD3D11Present(swapChain, interval, flags);
}

void __stdcall OnD3D11DeviceContextVSSetShader(
    ID3D11DeviceContext* context,
    ID3D11VertexShader* vertexShader,
    ID3D11ClassInstance* const *classInstances,
    UINT nClassInstances)
{
    PRINT("Vertex shader set");
    currentVertexShader = (void*)vertexShader;
    return phookD3D11DeviceContextVSSetShader(context, vertexShader, classInstances, nClassInstances);
}

void __stdcall OnD3D11DeviceContextPSSetShader(
    ID3D11DeviceContext* context,
    ID3D11PixelShader* pixelShader,
    ID3D11ClassInstance* const *classInstances,
    UINT nClassInstances)
{
    PRINT("Pixel shader set");
    currentPixelShader = (void*)pixelShader;
    return phookD3D11DeviceContextPSSetShader(context, pixelShader, classInstances, nClassInstances);
}

LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if(nCode == HC_ACTION)
    {
        switch(wParam)
        {
        case WM_KEYDOWN:
            PKBDLLHOOKSTRUCT p = (PKBDLLHOOKSTRUCT)lParam;
            if(p->vkCode >= 0x31 && p->vkCode <= 0x39) // 1
            {
                PRINT("Shader control");
                int index = p->vkCode - 0x31;
                if(index >= shaderDisableMap.size())
                    break;
                std::map<void*, bool>::iterator it =
                    shaderDisableMap.begin();
                for(int i = 0; i < index; ++i)
                    ++it;
                it->second = it->second ? false : true;
                if(it->second)
                {
                    PRINT("Shader enabled");
                }
                else
                {
                    PRINT("Shader disabled");
                }
            }
            break;
        }
    }
    
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

void InitD3D(HWND hWnd)
{
    HRESULT hr;
    
    ID3D11Device* device = NULL;
    D3D_FEATURE_LEVEL fl;
    ID3D11DeviceContext* deviceContext = NULL;
    IDXGISwapChain* swapChain = NULL;
    
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 1;
    sd.BufferDesc.Width = 640;
    sd.BufferDesc.Height = 480;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.SampleDesc.Count = 1;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    sd.Windowed = TRUE;
    
    hr = D3D11CreateDeviceAndSwapChain(
        NULL,
        D3D_DRIVER_TYPE_HARDWARE,
        NULL,
        D3D11_CREATE_DEVICE_PREVENT_ALTERING_LAYER_SETTINGS_FROM_REGISTRY,
        NULL,
        0,
        D3D11_SDK_VERSION,
        &sd,
        &swapChain,
        &device,
        &fl,
        &deviceContext
    );
    
    if(hr != S_OK)
    {
        PRINT("D3D11CreateDevice failed");
        return;
    }
    
    DWORD_PTR* swapChainVTable = (DWORD_PTR*)((DWORD_PTR*)swapChain)[0];
    DWORD_PTR* contextVTable = (DWORD_PTR*)((DWORD_PTR*)deviceContext)[0];
    
    if(MH_Initialize() != MH_OK)
    {
        PRINT("MH_Initialize failed");
        return;
    }
    
    MH_STATUS mhStatus;
    
    if(MH_CreateHook((DWORD_PTR*)swapChainVTable[SC_PRESENT], OnD3D11Present, (void**)&phookD3D11Present) != MH_OK)
    {
        PRINT("Failed to hook D3D11Present");
    }
    if(MH_EnableHook((DWORD_PTR*)swapChainVTable[SC_PRESENT]) != MH_OK)
    {
        PRINT("Failed to enable hook D3D11Present");
    }
    
    if(MH_CreateHook((DWORD_PTR*)contextVTable[VSSETSHADER], OnD3D11DeviceContextVSSetShader, (void**)&phookD3D11DeviceContextVSSetShader) != MH_OK)
    {
        PRINT("Failed to hook VSSetShader");
    }
    if(MH_EnableHook((DWORD_PTR*)contextVTable[VSSETSHADER]) != MH_OK)
    {
        PRINT("Failed to enable hook VSSetShader");
    }
    
    if(MH_CreateHook((DWORD_PTR*)contextVTable[PSSETSHADER], OnD3D11DeviceContextPSSetShader, (void**)&phookD3D11DeviceContextPSSetShader) != MH_OK)
    {
        PRINT("Failed to hook PSSetShader");
    }
    if(MH_EnableHook((DWORD_PTR*)contextVTable[PSSETSHADER]) != MH_OK)
    {
        PRINT("Failed to enable hook PSSetShader");
    }
    
    HHOOK hhkKeyboard = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, 0, 0);
    
    MSG msg = { 0 };
    while(!GetMessage(&msg, NULL, NULL, NULL))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    MH_Uninitialize();
    // C
    //device->lpVtbl->Release(device);
    //swapChain->lpVtbl->Release(swapChain);
    //deviceContext->lpVtbl->Release(deviceContext);
    device->Release();
    swapChain->Release();
    deviceContext->Release();
}

BOOL CALLBACK EnumThreadWndProc(HWND hWnd, LPARAM lParam)
{
    wchar_t title[256];
    GetWindowText(hWnd, title, sizeof(title));
    WPRINT(title);
    
    if(wcscmp(title, L"Prey") != 0)
        return FALSE;
    
    InitD3D(hWnd);
    
    return TRUE;
}

BOOL ListProcessThreads(DWORD pid)
{
    HANDLE threadSnap = INVALID_HANDLE_VALUE;
    THREADENTRY32 te32;
    
    threadSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if(threadSnap == INVALID_HANDLE_VALUE)
    {
        PRINT("CreateToolhelp32Snapshot failed");
        return FALSE;
    }
    
    te32.dwSize = sizeof(THREADENTRY32);
    
    if(!Thread32First(threadSnap, &te32))
    {
        PRINT("Thread32First failed");
        CloseHandle(threadSnap);
        return FALSE;
    }
    
    do
    {
        if(te32.th32OwnerProcessID == pid)
        {
            EnumThreadWindows(te32.th32ThreadID, &EnumThreadWndProc, 0);
        }
    } while(Thread32Next(threadSnap, &te32));
    
    CloseHandle(threadSnap);
    
    return TRUE;
}
extern "C"{
DLLEXPORT void Init(void)
{
    InitConsoleWindow();
    wchar_t moduleName[MAX_PATH];
    GetModuleFileName(NULL, moduleName, sizeof(wchar_t) * MAX_PATH);
    WPRINT(moduleName);
    PRINT("Hello");
    
    ListProcessThreads(GetCurrentProcessId());
}
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