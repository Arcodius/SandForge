//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#include "stdafx.h"
#include "Win32Application.h"

HWND Win32Application::m_hwnd = nullptr;

int Win32Application::Run(DXSample* pSample, HINSTANCE hInstance, int nCmdShow)
{
    if (pSample == nullptr)
    {
        return -1;
    }

    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv == nullptr)
    {
        return static_cast<int>(GetLastError());
    }
    pSample->ParseCommandLineArgs(argv, argc);
    LocalFree(argv);

    // Initialize the window class.
    WNDCLASSEX windowClass = { 0 };
    windowClass.cbSize = sizeof(WNDCLASSEX);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = hInstance;
    windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    windowClass.lpszClassName = L"DXSampleClass";
    if (RegisterClassEx(&windowClass) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
    {
        return static_cast<int>(GetLastError());
    }

    RECT windowRect = { 0, 0, static_cast<LONG>(pSample->GetWidth()), static_cast<LONG>(pSample->GetHeight()) };
    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

    // Create the window and store a handle to it.
    m_hwnd = CreateWindow(
        windowClass.lpszClassName,
        pSample->GetTitle(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        windowRect.right - windowRect.left,
        windowRect.bottom - windowRect.top,
        nullptr,        // We have no parent window.
        nullptr,        // We aren't using menus.
        hInstance,
        pSample);
    if (m_hwnd == nullptr)
    {
        return static_cast<int>(GetLastError());
    }

    // Initialize the sample. OnInit is defined in each child-implementation of DXSample.
    pSample->OnInit();

    ShowWindow(m_hwnd, nCmdShow);

    MSG msg = {};
    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            pSample->OnUpdate();
            pSample->OnRender();
        }
    }

    pSample->OnDestroy();

    // Return this part of the WM_QUIT message to Windows.
    m_hwnd = nullptr;
    return static_cast<int>(msg.wParam);
}

// Main message handler for the sample.
LRESULT CALLBACK Win32Application::WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    DXSample* pSample = reinterpret_cast<DXSample*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

    switch (message)
    {
    case WM_CREATE:
        {
            // Save the DXSample* passed in to CreateWindow.
            LPCREATESTRUCT pCreateStruct = reinterpret_cast<LPCREATESTRUCT>(lParam);
            SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pCreateStruct->lpCreateParams));
        }
        return 0;

    case WM_KEYDOWN:
        if (pSample)
        {
            pSample->OnKeyDown(static_cast<UINT8>(wParam));
        }
        return 0;

    case WM_KEYUP:
        if (pSample)
        {
            pSample->OnKeyUp(static_cast<UINT8>(wParam));
        }
        return 0;

    case WM_MOUSEMOVE:
        if (pSample)
        {
            pSample->OnMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        }
        return 0;
    case WM_LBUTTONDOWN:
        SetCapture(hWnd);
        if (pSample) {
            pSample->OnMouseButton(MouseButton::Left, true, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        }
        return 0;
    case WM_LBUTTONUP:
        // wParam holds the buttons still down: keep capture while the other button is held
        if (GetCapture() == hWnd && !(wParam & MK_RBUTTON)) {
            ReleaseCapture();
        }
        if (pSample) {
            pSample->OnMouseButton(MouseButton::Left, false, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        }
        return 0;

    case WM_RBUTTONDOWN:
        SetCapture(hWnd);
        if (pSample)
        {
            pSample->OnMouseButton(MouseButton::Right, true, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        }
        return 0;

    case WM_RBUTTONUP:
        if (GetCapture() == hWnd && !(wParam & MK_LBUTTON))
        {
            ReleaseCapture();
        }
        if (pSample)
        {
            pSample->OnMouseButton(MouseButton::Right, false, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        }
        return 0;

    case WM_CAPTURECHANGED:
        // Capture lost (alt-tab, another window...): no BUTTONUP will arrive, so release both buttons here
        if (pSample)
        {
            pSample->OnMouseButton(MouseButton::Left, false, 0, 0);
            pSample->OnMouseButton(MouseButton::Right, false, 0, 0);
        }
        return 0;

    case WM_MOUSEWHEEL:
        if (pSample)
        {
            pSample->OnMouseWheel(GET_WHEEL_DELTA_WPARAM(wParam));
        }
        return 0;

    case WM_KILLFOCUS:
        if (GetCapture() == hWnd)
        {
            ReleaseCapture();
        }
        if (pSample)
        {
            pSample->OnMouseButton(MouseButton::Left, false, 0, 0);
            pSample->OnMouseButton(MouseButton::Right, false, 0, 0);
            for (UINT8 key = 0; key < 255; ++key)
            {
                pSample->OnKeyUp(key);
            }
        }
        return 0;

    case WM_PAINT:
        {
            PAINTSTRUCT paintStruct = {};
            BeginPaint(hWnd, &paintStruct);
            EndPaint(hWnd, &paintStruct);
        }
        return 0;

    case WM_SIZE:
        if (pSample)
        {
            pSample->OnResize(LOWORD(lParam), HIWORD(lParam));
        }
        return 0;

    case WM_ERASEBKGND:
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    // Handle any messages the switch statement didn't.
    return DefWindowProc(hWnd, message, wParam, lParam);
}
