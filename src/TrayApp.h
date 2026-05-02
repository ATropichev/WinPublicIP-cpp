#pragma once
#include <windows.h>
#include <string>
#include "Config/AppSettings.h"
#include "Services/IpProvider.h"
#include "Services/GeoProvider.h"
#include "Services/VpnDetector.h"
#include "Rendering/TrayRenderer.h"

#define WM_APP_TRAY      (WM_APP + 1)
#define WM_APP_REFRESH   (WM_APP + 2)

// Идентификаторы пунктов меню
enum MenuId {
    ID_REFRESH  = 1001,
    ID_COPY_IP  = 1002,
    ID_AUTORUN  = 1003,
    ID_SETTINGS = 1004,
    ID_QUIT     = 1005,
};

class TrayApp {
public:
    explicit TrayApp(HINSTANCE hInstance);
    ~TrayApp();
    void Run();

private:
    HINSTANCE    hInstance_;
    HWND         hWnd_       = nullptr;
    AppSettings  settings_;
    IpProvider   ipProvider_;
    GeoProvider  geoProvider_;
    VpnDetector  vpnDetector_;
    TrayRenderer renderer_;
    UINT_PTR     timerId_    = 0;
    std::string  lastIp_;
    UINT         wmTaskbarCreated_ = 0;

    void CreateWindow_();
    void AddTrayIcon();
    void UpdateTrayIcon(HICON icon, const std::wstring& tooltip);
    void RemoveTrayIcon();
    void ShowContextMenu();
    void CopyIpToClipboard();
    void StartRefreshThread();
    void OnRefreshDone(WPARAM wp, LPARAM lp);
    void OpenSettings();

    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
};
