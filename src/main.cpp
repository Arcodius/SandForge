#include "stdafx.h"
#include "app/SandForge.h"

_Use_decl_annotations_
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
    SandForge sample(1280, 720, L"SandForge");
    return Win32Application::Run(&sample, hInstance, nCmdShow);
}