#include <windows.h>
#include <shellapi.h>
#include <thread>
#include <string>
#include <fstream>
#include <stdexcept>
#include "TrayApp.h"
#include "SettingsDialog.h"

static void Log(const std::string& msg)
{
    char tmp[MAX_PATH];
    GetTempPathA(MAX_PATH, tmp);
    std::ofstream f(std::string(tmp) + "WinPublicIP.log", std::ios::app);
    f << msg << "\n";
}

static const wchar_t* WND_CLASS = L"WinPublicIPHost";
static const UINT     TRAY_ID   = 1;

// Структура передаётся из рабочего потока в UI через PostMessage
struct RefreshResult {
    bool        success;
    std::string ip;
    GeoResult   geo;
    bool        vpnOn;
    std::string error;
};

static std::wstring ToWide(const std::string& s)
{
    if (s.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w(len, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), w.data(), len);
    return w;
}

// Обрезка до maxLen символов
static std::wstring Trunc(const std::wstring& s, size_t maxLen)
{
    return s.size() <= maxLen ? s : s.substr(0, maxLen);
}

TrayApp::TrayApp(HINSTANCE hInstance)
    : hInstance_(hInstance)
    , settings_(AppSettings::Load())
    , ipProvider_(settings_.ipProviders)
    , vpnDetector_(settings_.vpnInterfacePatterns)
{
    wmTaskbarCreated_ = RegisterWindowMessageW(L"TaskbarCreated");
    CreateWindow_();
    AddTrayIcon();
    // Первое обновление сразу
    StartRefreshThread();
    // Таймер для периодического обновления
    timerId_ = SetTimer(hWnd_, 1, settings_.refreshIntervalSeconds * 1000, nullptr);
}

TrayApp::~TrayApp()
{
    if (timerId_) KillTimer(hWnd_, timerId_);
    RemoveTrayIcon();
    DestroyWindow(hWnd_);
}

void TrayApp::CreateWindow_()
{
    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance_;
    wc.lpszClassName = WND_CLASS;
    RegisterClassExW(&wc);

    hWnd_ = CreateWindowExW(0, WND_CLASS, L"WinPublicIP",
        0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, hInstance_, this);
}

void TrayApp::AddTrayIcon()
{
    NOTIFYICONDATAW nid{};
    nid.cbSize           = sizeof(nid);
    nid.hWnd             = hWnd_;
    nid.uID              = TRAY_ID;
    nid.uFlags           = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    nid.uCallbackMessage = WM_APP_TRAY;
    nid.hIcon            = renderer_.RenderOffline();
    wcsncpy_s(nid.szTip, L"WinPublicIP — запуск...", _TRUNCATE);
    Shell_NotifyIconW(NIM_ADD, &nid);
}

void TrayApp::UpdateTrayIcon(HICON icon, const std::wstring& tooltip)
{
    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd   = hWnd_;
    nid.uID    = TRAY_ID;
    nid.uFlags = NIF_ICON | NIF_TIP;
    nid.hIcon  = icon;
    wcsncpy_s(nid.szTip, Trunc(tooltip, 127).c_str(), _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

void TrayApp::RemoveTrayIcon()
{
    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd   = hWnd_;
    nid.uID    = TRAY_ID;
    Shell_NotifyIconW(NIM_DELETE, &nid);
}

void TrayApp::ShowContextMenu()
{
    HMENU hMenu = CreatePopupMenu();

    // Отображение IP (disabled)
    std::wstring ipLabel = lastIp_.empty() ? L"IP: —" : L"IP: " + ToWide(lastIp_);
    AppendMenuW(hMenu, MF_STRING | MF_DISABLED, 0, ipLabel.c_str());
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);

    AppendMenuW(hMenu, MF_STRING, ID_REFRESH,  L"Обновить сейчас");
    AppendMenuW(hMenu, MF_STRING, ID_COPY_IP,  L"Копировать IP");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);

    UINT autorunFlags = MF_STRING | (settings_.startWithWindows ? MF_CHECKED : 0);
    AppendMenuW(hMenu, autorunFlags, ID_AUTORUN, L"Запускать с Windows");
    AppendMenuW(hMenu, MF_STRING,    ID_SETTINGS, L"Настройки...");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING,    ID_QUIT, L"Выход");

    // Меню должно закрываться при клике вне его
    SetForegroundWindow(hWnd_);
    POINT pt;
    GetCursorPos(&pt);
    TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN,
                   pt.x, pt.y, 0, hWnd_, nullptr);
    DestroyMenu(hMenu);
}

void TrayApp::CopyIpToClipboard()
{
    if (lastIp_.empty()) return;
    std::wstring wip = ToWide(lastIp_);
    size_t bytes = (wip.size() + 1) * sizeof(wchar_t);
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes);
    memcpy(GlobalLock(hMem), wip.c_str(), bytes);
    GlobalUnlock(hMem);
    OpenClipboard(hWnd_);
    EmptyClipboard();
    SetClipboardData(CF_UNICODETEXT, hMem);
    CloseClipboard();
}

void TrayApp::StartRefreshThread()
{
    std::thread([this]() {
        auto* result = new RefreshResult();
        try {
            result->ip    = ipProvider_.Get();
            Log("IP: " + result->ip);
            result->geo   = geoProvider_.Get(result->ip);
            Log("Country: " + result->geo.country + " (" + result->geo.countryCode + ")");
            result->vpnOn = vpnDetector_.IsActive();
            Log("VPN: " + std::string(result->vpnOn ? "on" : "off"));
            result->success = true;
        } catch (const std::exception& e) {
            result->success = false;
            result->error   = e.what();
            Log("ERROR: " + std::string(e.what()));
        }
        // Передаём результат в UI-поток
        PostMessageW(hWnd_, WM_APP_REFRESH, 0,
                     reinterpret_cast<LPARAM>(result));
    }).detach();
}

void TrayApp::OnRefreshDone(WPARAM, LPARAM lp)
{
    auto* result = reinterpret_cast<RefreshResult*>(lp);

    if (result->success) {
        bool ipChanged = result->ip != lastIp_;
        lastIp_ = result->ip;

        HICON icon = renderer_.Render(result->geo.countryCode, result->vpnOn);
        std::wstring vpnStr = result->vpnOn ? L"on" : L"off";
        std::wstring tooltip =
            L"IP: " + ToWide(result->ip) +
            L" | " + ToWide(result->geo.country) +
            L" | VPN: " + vpnStr;
        UpdateTrayIcon(icon, tooltip);

        // Balloon при смене IP
        if (settings_.notifyOnIpChange && ipChanged && !lastIp_.empty()) {
            NOTIFYICONDATAW nid{};
            nid.cbSize    = sizeof(nid);
            nid.hWnd      = hWnd_;
            nid.uID       = TRAY_ID;
            nid.uFlags    = NIF_INFO;
            nid.dwInfoFlags = NIIF_INFO;
            wcsncpy_s(nid.szInfo,     ToWide(result->ip).c_str(), _TRUNCATE);
            wcsncpy_s(nid.szInfoTitle, L"IP изменился", _TRUNCATE);
            Shell_NotifyIconW(NIM_MODIFY, &nid);
        }
    } else {
        HICON icon = renderer_.RenderOffline();
        UpdateTrayIcon(icon, L"Нет подключения");
    }
    delete result;
}

void TrayApp::OpenSettings()
{
    int oldInterval = settings_.refreshIntervalSeconds;
    if (ShowSettingsDialog(hWnd_, hInstance_, settings_)) {
        settings_.Save();
        if (settings_.refreshIntervalSeconds != oldInterval) {
            KillTimer(hWnd_, timerId_);
            timerId_ = SetTimer(hWnd_, 1,
                settings_.refreshIntervalSeconds * 1000, nullptr);
        }
    }
}

void TrayApp::Run()
{
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

LRESULT CALLBACK TrayApp::WndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp)
{
    auto* app = reinterpret_cast<TrayApp*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));

    if (msg == WM_CREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA,
            reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
        return 0;
    }

    if (!app) return DefWindowProcW(hWnd, msg, wp, lp);

    if (msg == app->wmTaskbarCreated_) {
        app->AddTrayIcon();
        return 0;
    }

    switch (msg) {
    case WM_APP_TRAY:
        if (lp == WM_RBUTTONUP || lp == WM_CONTEXTMENU)
            app->ShowContextMenu();
        return 0;

    case WM_APP_REFRESH:
        app->OnRefreshDone(wp, lp);
        return 0;

    case WM_TIMER:
        app->StartRefreshThread();
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case ID_REFRESH:  app->StartRefreshThread();   break;
        case ID_COPY_IP:  app->CopyIpToClipboard();    break;
        case ID_SETTINGS: app->OpenSettings();         break;
        case ID_AUTORUN:
            app->settings_.startWithWindows = !app->settings_.startWithWindows;
            app->settings_.SetAutorun(app->settings_.startWithWindows);
            app->settings_.Save();
            break;
        case ID_QUIT:
            PostQuitMessage(0);
            break;
        }
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wp, lp);
}
