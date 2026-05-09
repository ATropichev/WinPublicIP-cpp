#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <exception>
#include "TrayApp.h"

static const wchar_t* MUTEX_NAME =
    L"Local\\ATropichev.WinPublicIP.{D8F3C4A7-7F2E-4B7A-9D4B-0B5B4B7B6F30}";

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
    // Защита от повторного запуска
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, MUTEX_NAME);
    if (!hMutex) {
        MessageBoxW(nullptr,
            L"Не удалось создать блокировку единственного экземпляра WinPublicIP.",
            L"WinPublicIP",
            MB_OK | MB_ICONERROR);
        return 1;
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxW(nullptr,
            L"WinPublicIP уже запущен.\n\nПроверьте значок приложения в системном трее.",
            L"WinPublicIP",
            MB_OK | MB_ICONINFORMATION);
        CloseHandle(hMutex);
        return 0;
    }

    // Инициализация GDI+
    Gdiplus::GdiplusStartupInput gdipInput;
    ULONG_PTR gdipToken = 0;
    if (Gdiplus::GdiplusStartup(&gdipToken, &gdipInput, nullptr) != Gdiplus::Ok) {
        MessageBoxW(nullptr,
            L"Не удалось инициализировать графическую подсистему WinPublicIP.",
            L"WinPublicIP",
            MB_OK | MB_ICONERROR);
        CloseHandle(hMutex);
        return 1;
    }

    // Запуск приложения
    try {
        TrayApp app(hInstance);
        app.Run();
    } catch (const std::exception& e) {
        MessageBoxA(nullptr, e.what(), "WinPublicIP", MB_OK | MB_ICONERROR);
        Gdiplus::GdiplusShutdown(gdipToken);
        CloseHandle(hMutex);
        return 1;
    } catch (...) {
        MessageBoxW(nullptr,
            L"Не удалось запустить WinPublicIP.",
            L"WinPublicIP",
            MB_OK | MB_ICONERROR);
        Gdiplus::GdiplusShutdown(gdipToken);
        CloseHandle(hMutex);
        return 1;
    }

    // Завершение GDI+
    Gdiplus::GdiplusShutdown(gdipToken);
    CloseHandle(hMutex);
    return 0;
}
