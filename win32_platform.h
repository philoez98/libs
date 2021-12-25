#pragma once

#include <Windows.h>
#include <d3d11.h>

struct window
{
    wchar_t Name[64];
 
    HWND Handle;

    int Width;
    int Height;

    int ScreenX;
    int ScreenY;

    int ShouldClose;
    int HasFocus;
    int IsMinimized;
    int HasResized;
};

window* CreateWin32Window(const wchar_t* Name, int Width, int Height);
window* CreateWin32WindowEx(const wchar_t* Name, int Width, int Height, WNDPROC WindowProc);

void DestroyWin32Window(window* Window);

LRESULT CALLBACK DefaultWin32WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void PumpWindowMessages(window* Window);

struct gpu_context
{
    ID3D11Device* Device;
    ID3D11DeviceContext* DeviceContext;

    IDXGISwapChain* Swapchain;
    ID3D11RenderTargetView* BackBuffer;
    
    D3D_FEATURE_LEVEL FeatureLevel;

    int IsInitialized;
    int UseSRGBFormat;
};

gpu_context* InitializeGraphics(HWND Window);
gpu_context* InitializeGraphicsEx(HWND Window, int SRGB, int FormatBGRA);

int ResizeSwapchain(gpu_context* Context, int Width, int Height);
void DestroyGraphics(gpu_context* Context);



#ifdef WIN32_PLATFORM_IMPLEMENTATION

#include <assert.h>
#include <string.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

#ifndef ZeroStruct
#define ZeroStruct(s) memset(&(s), 0, sizeof(s))
#endif

LRESULT CALLBACK DefaultWin32WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    window* Window = (window*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch(uMsg)
    {
        case WM_CREATE:
        {
            if (Window)
            {
                RECT Rect;
                GetClientRect(hwnd, &Rect);

                Window->Width = Rect.right - Rect.left;
                Window->Height = Rect.bottom - Rect.top;
            }
        } return 0;

        case WM_DESTROY:
        {
            PostQuitMessage(0);
        } return 0;

        case WM_CLOSE:
        {
            if (Window)
            {
                Window->ShouldClose = 1;
            }
        } return 0;

        case WM_SETFOCUS:
        {
            if (Window)
            {
                Window->HasFocus = 1;
            }
        } return 0;

        case WM_KILLFOCUS:
        {
            if (Window)
            {
                Window->HasFocus = 0;
            }
        } return 0;

        case WM_ACTIVATE:
        {
            if (Window)
            {
                Window->HasFocus = (LOWORD(wParam) != WA_INACTIVE);
            }
        } return 0;

        case WM_MOVE:
        {
            if (Window)
            {
                Window->ScreenX = (int)(short)LOWORD(lParam);
                Window->ScreenY = (int)(short)HIWORD(lParam);
            }
        } return 0;

        case WM_SIZE:
        {
            if (Window)
            {
                int Width = LOWORD(lParam);
                int Height = HIWORD(lParam);

                Window->Height = Height;
                Window->Width = Width;

                Window->HasResized = 1;
                Window->IsMinimized = (wParam == SIZE_MINIMIZED);
            }
        } return 0;
    }
    
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

window* CreateWin32Window(const wchar_t* Name, int Width, int Height)
{
    return CreateWin32WindowEx(Name, Width, Height, NULL);
}

window* CreateWin32WindowEx(const wchar_t* Name, int Width, int Height, WNDPROC WindowProc)
{
    const wchar_t* WindowName = Name ? Name : L"Win32 Window";

    int WindowWidth = (Width > 0) ? Width : 1920;
    int WindowHeight = (Height > 0) ? Height : 1080;

    HINSTANCE Instance = GetModuleHandleW(NULL);

    WNDCLASSEXW WindowClass; ZeroStruct(WindowClass);
    WindowClass.cbSize = sizeof(WNDCLASSEXW);
    WindowClass.style = CS_HREDRAW | CS_VREDRAW;
    WindowClass.hInstance = Instance;
    WindowClass.lpfnWndProc = WindowProc ? WindowProc : DefaultWin32WindowProc;
    WindowClass.lpszClassName = WindowName;
    WindowClass.hCursor = LoadCursorW(0, IDC_ARROW);

    if (!RegisterClassExW(&WindowClass))
    {
        return NULL;
    }

    // TODO: different window styles
    RECT Rect = { 0, 0, WindowWidth, WindowHeight };
    AdjustWindowRect(&Rect, WS_OVERLAPPEDWINDOW | WS_BORDER, FALSE);

    HWND Handle = CreateWindowW(WindowClass.lpszClassName,
        WindowName, WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_BORDER,
        CW_USEDEFAULT, CW_USEDEFAULT, Rect.right - Rect.left,
        Rect.bottom - Rect.top,
        0, 0, Instance, 0);

    window* Window;
    if (Handle)
    {
        Window = (window*)calloc(1, sizeof(window));
        if (!Window)
        {
            DestroyWindow(Handle);
            return NULL;
        }

        wcscpy_s(Window->Name, sizeof(Window->Name) / sizeof(wchar_t), WindowName);

        Window->Handle = Handle;
        Window->Width = WindowWidth;
        Window->Height = WindowHeight;

        SetWindowLongPtrW(Window->Handle, GWLP_USERDATA, (LONG_PTR)Window);

        ShowWindow(Window->Handle, SW_SHOWNORMAL);
        UpdateWindow(Window->Handle);
    }

    return Window;
}

void DestroyWin32Window(window* Window)
{
    if (Window && Window->Handle)
    {
        DestroyWindow(Window->Handle);
        free(Window);
    }
}

void PumpWindowMessages(window* Window)
{
    MSG Message; ZeroStruct(Message);
    while (PeekMessageW(&Message, Window->Handle, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&Message);
        DispatchMessageW(&Message);
    }
}

static IDXGIAdapter* GetBestAvailableAdapter()
{
    IDXGIFactory1* Factory = NULL;
    HRESULT Result = CreateDXGIFactory1(IID_PPV_ARGS(&Factory));

    if (FAILED(Result))
    {
        return NULL;
    }

    int AdapterCount = 0;
    IDXGIAdapter* Adapter = 0;
    IDXGIAdapter* Adapters[16] = {0};

    while (SUCCEEDED(Factory->EnumAdapters(AdapterCount, &Adapter)))
    {
        Adapters[AdapterCount++] = Adapter;
    }

    Factory->Release();

    if (!AdapterCount)
    {
        return NULL;
    }

    // Get the best device (best as in with the most video memory)
    DXGI_ADAPTER_DESC Desc;
    ZeroStruct(Desc);

    int BestAdapter = 0;
    unsigned long int MaxVidMem = 0;
    for (int j = 0; j < AdapterCount; ++j) {
        IDXGIAdapter* CurrentAdapter = Adapters[j];
        
        if (FAILED(CurrentAdapter->GetDesc(&Desc)))
        {
            continue;
        }

        if (Desc.DedicatedVideoMemory > MaxVidMem) {
            BestAdapter = j;
            MaxVidMem = Desc.DedicatedVideoMemory;
        }
    }

    return Adapters[BestAdapter];
}

static bool CreateSwapchain(HWND Window, gpu_context* Context, int Width, int Height, bool UseBGRA)
{
    IDXGIDevice* DXGIDevice = NULL;
    HRESULT Result = Context->Device->QueryInterface(__uuidof(IDXGIDevice), (void**)&DXGIDevice);

    if (FAILED(Result))
    {
        return false;
    }

    IDXGIAdapter* DXGIAdapter = NULL;
    Result = DXGIDevice->GetParent(__uuidof(IDXGIAdapter), (void**)&DXGIAdapter);

    if (FAILED(Result))
    {
        return false;
    }

    IDXGIFactory* IDXGIFactory = NULL;
    Result = DXGIAdapter->GetParent(__uuidof(IDXGIFactory), (void**)&IDXGIFactory);

    if (FAILED(Result))
    {
        return false;
    }

    DXGI_MODE_DESC DisplayMode; ZeroStruct(DisplayMode);
    DisplayMode.Width = Width;
    DisplayMode.Height = Height;

    if (!UseBGRA)
    {
        DisplayMode.Format = Context->UseSRGBFormat ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
    }
    else
    {
        DisplayMode.Format = Context->UseSRGBFormat ? DXGI_FORMAT_B8G8R8A8_UNORM_SRGB : DXGI_FORMAT_B8G8R8A8_UNORM;
    }

    DisplayMode.RefreshRate = { 60, 1 }; // TODO: allow different refresh rates

    // Create the swapchain
    DXGI_SWAP_CHAIN_DESC Desc;
    ZeroStruct(Desc);
    
    Desc.BufferCount = 2;
    Desc.BufferDesc = DisplayMode;
    Desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    Desc.SampleDesc.Count = 1;
    Desc.OutputWindow = Window;
    Desc.Windowed = true;
    Desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    Result = IDXGIFactory->CreateSwapChain(Context->Device, &Desc, &Context->Swapchain);

    if (FAILED(Result))
    {
        // Windows 8.1
        Desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        Result = IDXGIFactory->CreateSwapChain(Context->Device, &Desc, &Context->Swapchain);

        if (FAILED(Result))
        {
            // Older Windows
            Desc.BufferCount = 1;
            Desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
            Result = IDXGIFactory->CreateSwapChain(Context->Device, &Desc, &Context->Swapchain);
        }
    }

    DXGIDevice->Release();
    DXGIAdapter->Release();
    IDXGIFactory->Release();

    if (FAILED(Result))
    {
        return false;
    }

    ID3D11Texture2D* Buffer = NULL;
    Result = Context->Swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&Buffer);

    if (FAILED(Result))
    {
        return false;
    }

    Result = Context->Device->CreateRenderTargetView(Buffer, NULL, &Context->BackBuffer);
    Buffer->Release();

    if (FAILED(Result))
    {
        return false;
    }

    return true;
}

gpu_context* InitializeGraphics(HWND Window)
{
    return InitializeGraphicsEx(Window, 0, 0);
}

gpu_context* InitializeGraphicsEx(HWND Window, int SRGB, int FormatBGRA)
{
    IDXGIAdapter* Adapter = GetBestAvailableAdapter();
    if (!Adapter)
    {
        return NULL;
    }
    
    const D3D_FEATURE_LEVEL FeatureLevels[] =
    {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0
    };

    unsigned int DeviceFlags = 0;

#ifdef _DEBUG
    DeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    gpu_context* Context = (gpu_context*)calloc(1, sizeof(gpu_context));
    if (!Context)
    {
        return NULL;
    }

    HRESULT Result = D3D11CreateDevice(Adapter,
                                       Adapter ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE,
                                       0,
                                       DeviceFlags,
                                       FeatureLevels,
                                       sizeof(FeatureLevels) / sizeof(D3D_FEATURE_LEVEL),
                                       D3D11_SDK_VERSION,
                                       &Context->Device,
                                       &Context->FeatureLevel,
                                       &Context->DeviceContext);

    if (FAILED(Result))
    {

        // Create a WARP device
        Result = D3D11CreateDevice(Adapter, D3D_DRIVER_TYPE_WARP, 0,
            DeviceFlags, FeatureLevels, sizeof(FeatureLevels) / sizeof(D3D_FEATURE_LEVEL), D3D11_SDK_VERSION,
            &Context->Device, &Context->FeatureLevel, &Context->DeviceContext);

        if (FAILED(Result))
        {
            free(Context);
            return NULL;
        }
    }

    Context->UseSRGBFormat = SRGB;

    RECT Rect; ZeroStruct(Rect);
    GetClientRect(Window, &Rect);

    Context->IsInitialized = CreateSwapchain(Window, Context, Rect.right - Rect.left, Rect.bottom - Rect.top, FormatBGRA);

    return Context;
}

int ResizeSwapchain(gpu_context* Context, int Width, int Height)
{
    if (!Context || !Context->IsInitialized || Width <= 0 || Height <= 0)
    {
        return 0;
    }

    DXGI_SWAP_CHAIN_DESC Desc; ZeroStruct(Desc);
    Context->Swapchain->GetDesc(&Desc);

    if (Desc.BufferDesc.Width == Width || Desc.BufferDesc.Height == Height)
    {
        return 0;
    }

    Context->DeviceContext->ClearState();
    Context->DeviceContext->Flush();

    Context->BackBuffer->Release();

    HRESULT Result = Context->Swapchain->ResizeBuffers(0, Width, Height, DXGI_FORMAT_UNKNOWN, Desc.Flags);
    if (FAILED(Result))
    {
        return 0;
    }

    ID3D11Texture2D* Buffer = NULL;
    Result = Context->Swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&Buffer);

    if (FAILED(Result))
    {
        return 0;
    }

    Result = Context->Device->CreateRenderTargetView(Buffer, NULL, &Context->BackBuffer);
    Buffer->Release();

    if (FAILED(Result))
    {
        return 0;
    }

    return 1;
}

void DestroyGraphics(gpu_context* Context)
{
    if (Context)
    {
        if (Context->BackBuffer)
        {
            Context->BackBuffer->Release();
        }

        if (Context->Swapchain)
        {
            Context->Swapchain->Release();
        }

        if (Context->DeviceContext)
        {
            Context->DeviceContext->Release();
        }

        if (Context->Device)
        {
            Context->Device->Release();
        }

        free(Context);
    }
}

#endif
