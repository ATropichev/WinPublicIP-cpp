#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include "TrayApp.h"

static const wchar_t* MUTEX_NAME = L"WinPublicIP_SingleInstance";

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
    // Защита от повторного запуска
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, MUTEX_NAME);
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(hMutex);
        return 0;
    }

    // Инициализация GDI+
    Gdiplus::GdiplusStartupInput gdipInput;
    ULONG_PTR gdipToken;
    Gdiplus::GdiplusStartup(&gdipToken, &gdipInput, nullptr);

    // Запуск приложения
    {
        TrayApp app(hInstance);
        app.Run();
    }

    // Завершение GDI+
    Gdiplus::GdiplusShutdown(gdipToken);
    CloseHandle(hMutex);
    return 0;
}
