#include "stdafx.h"
#include "app/SandForge.h"

_Use_decl_annotations_
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
    try
    {
        SandForge sample(1280, 720, L"SandForge");
        return Win32Application::Run(&sample, hInstance, nCmdShow);
    }
    catch (const HrException& error)
    {
        MessageBoxA(nullptr, error.what(), "SandForge Direct3D error", MB_OK | MB_ICONERROR);
    }
    catch (const std::exception& error)
    {
        MessageBoxA(nullptr, error.what(), "SandForge error", MB_OK | MB_ICONERROR);
    }

    return -1;
}