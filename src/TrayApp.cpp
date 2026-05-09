#include <windows.h>
#include <shellapi.h>
#include <cstdint>
#include <memory>
#include <thread>
#include <string>
#include <stdexcept>
#include "TrayApp.h"
#include "SettingsDialog.h"

static const wchar_t* WND_CLASS = L"WinPublicIPHost";
static const UINT     TRAY_ID   = 1;

static void DebugLog(const wchar_t* message)
{
    OutputDebugStringW(L"WinPublicIP: ");
    OutputDebugStringW(message);
    OutputDebugStringW(L"\n");
}

// Структура передаётся из рабочего потока в UI через PostMessage
struct RefreshResult {
    std::uint64_t seq = 0;
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
    , geoProvider_(settings_.geoProvider)
    , vpnDetector_(settings_.vpnInterfacePatterns)
{
    wmTaskbarCreated_ = RegisterWindowMessageW(L"TaskbarCreated");
    CreateWindow_();
    if (!hWnd_)
        throw std::runtime_error("failed to create tray host window");
    try {
        if (!AddTrayIcon())
            throw std::runtime_error("failed to add tray icon");
        // Первое обновление сразу
        StartRefreshThread();
        // Таймер для периодического обновления
        timerId_ = SetTimer(hWnd_, 1, settings_.refreshIntervalSeconds * 1000, nullptr);
        if (!timerId_)
            DebugLog(L"failed to create refresh timer");
    } catch (...) {
        shuttingDown_.store(true);
        if (refreshThread_.joinable())
            refreshThread_.join();
        RemoveTrayIcon();
        DestroyWindow(hWnd_);
        hWnd_ = nullptr;
        throw;
    }
}

TrayApp::~TrayApp()
{
    shuttingDown_.store(true);
    if (timerId_) KillTimer(hWnd_, timerId_);
    if (refreshThread_.joinable())
        refreshThread_.join();
    MSG msg;
    while (PeekMessageW(&msg, hWnd_, WM_APP_REFRESH, WM_APP_REFRESH, PM_REMOVE))
        delete reinterpret_cast<RefreshResult*>(msg.lParam);
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
    if (!RegisterClassExW(&wc))
        DebugLog(L"failed to register tray window class");

    hWnd_ = CreateWindowExW(0, WND_CLASS, L"WinPublicIP",
        0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, hInstance_, this);
    if (!hWnd_)
        DebugLog(L"failed to create tray host window");
}

bool TrayApp::AddTrayIcon()
{
    HICON icon = trayOnline_
        ? renderer_.Render(trayCountryCode_, trayVpnOn_)
        : renderer_.RenderOffline();

    NOTIFYICONDATAW nid{};
    nid.cbSize           = sizeof(nid);
    nid.hWnd             = hWnd_;
    nid.uID              = TRAY_ID;
    nid.uFlags           = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    nid.uCallbackMessage = WM_APP_TRAY;
    nid.hIcon            = icon;
    wcsncpy_s(nid.szTip, Trunc(trayTooltip_, 127).c_str(), _TRUNCATE);
    if (!Shell_NotifyIconW(NIM_ADD, &nid)) {
        DebugLog(L"failed to add tray icon");
        return false;
    }
    trayIconAdded_ = true;
    return true;
}

bool TrayApp::RestoreTrayIcon()
{
    trayIconAdded_ = false;
    if (!AddTrayIcon()) {
        DebugLog(L"failed to restore tray icon after Explorer restart");
        return false;
    }
    return true;
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
    if (!Shell_NotifyIconW(NIM_MODIFY, &nid))
        DebugLog(L"failed to update tray icon");
}

void TrayApp::RemoveTrayIcon()
{
    if (!trayIconAdded_)
        return;
    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd   = hWnd_;
    nid.uID    = TRAY_ID;
    if (!Shell_NotifyIconW(NIM_DELETE, &nid))
        DebugLog(L"failed to remove tray icon");
    trayIconAdded_ = false;
}

void TrayApp::ShowContextMenu()
{
    settings_.startWithWindows = settings_.IsAutorunEnabled();

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
    PostMessageW(hWnd_, WM_NULL, 0, 0);
    DestroyMenu(hMenu);
}

void TrayApp::CopyIpToClipboard()
{
    if (lastIp_.empty()) return;
    std::wstring wip = ToWide(lastIp_);
    size_t bytes = (wip.size() + 1) * sizeof(wchar_t);
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (!hMem) return;
    void* dest = GlobalLock(hMem);
    if (!dest) {
        GlobalFree(hMem);
        return;
    }
    memcpy(dest, wip.c_str(), bytes);
    GlobalUnlock(hMem);
    if (!OpenClipboard(hWnd_)) {
        GlobalFree(hMem);
        return;
    }
    if (!EmptyClipboard()) {
        CloseClipboard();
        GlobalFree(hMem);
        return;
    }
    if (SetClipboardData(CF_UNICODETEXT, hMem))
        hMem = nullptr;
    CloseClipboard();
    if (hMem)
        GlobalFree(hMem);
}

void TrayApp::StartRefreshThread()
{
    if (shuttingDown_.load())
        return;

    if (refreshThread_.joinable()) {
        if (refreshInProgress_.load())
            return;
        refreshThread_.join();
    }

    bool expected = false;
    if (!refreshInProgress_.compare_exchange_strong(expected, true))
        return;

    std::uint64_t seq = ++nextRefreshSeq_;
    latestRefreshSeq_ = seq;

    refreshThread_ = std::thread([this, seq]() {
        auto result = std::make_unique<RefreshResult>();
        result->seq = seq;
        try {
            result->ip      = ipProvider_.Get();
            result->geo     = geoProvider_.Get(result->ip);
            result->vpnOn   = vpnDetector_.IsActive();
            result->success = true;
        } catch (const std::exception& e) {
            result->success = false;
            result->error   = e.what();
        }
        // Передаём результат в UI-поток
        if (shuttingDown_.load() || !IsWindow(hWnd_) ||
            !PostMessageW(hWnd_, WM_APP_REFRESH, 0,
                          reinterpret_cast<LPARAM>(result.get()))) {
            refreshInProgress_.store(false);
            return;
        }
        result.release();
    });
}

void TrayApp::OnRefreshDone(WPARAM, LPARAM lp)
{
    std::unique_ptr<RefreshResult> result(reinterpret_cast<RefreshResult*>(lp));

    if (result->seq != latestRefreshSeq_) {
        refreshInProgress_.store(false);
        return;
    }

    if (result->success) {
        bool hadPreviousIp = !lastIp_.empty();
        bool ipChanged = hadPreviousIp && result->ip != lastIp_;
        lastIp_ = result->ip;

        HICON icon = renderer_.Render(result->geo.countryCode, result->vpnOn);
        std::wstring vpnStr = result->vpnOn ? L"on" : L"off";
        std::wstring tooltip =
            L"IP: " + ToWide(result->ip) +
            L"\n" + ToWide(result->geo.country) +
            L"\nVPN: " + vpnStr;
        trayOnline_ = true;
        trayCountryCode_ = result->geo.countryCode;
        trayVpnOn_ = result->vpnOn;
        trayTooltip_ = tooltip;
        UpdateTrayIcon(icon, tooltip);

        // Balloon при смене IP
        if (settings_.notifyOnIpChange && ipChanged) {
            NOTIFYICONDATAW nid{};
            nid.cbSize    = sizeof(nid);
            nid.hWnd      = hWnd_;
            nid.uID       = TRAY_ID;
            nid.uFlags    = NIF_INFO;
            nid.dwInfoFlags = NIIF_INFO;
            wcsncpy_s(nid.szInfo,     ToWide(result->ip).c_str(), _TRUNCATE);
            wcsncpy_s(nid.szInfoTitle, L"IP изменился", _TRUNCATE);
            if (!Shell_NotifyIconW(NIM_MODIFY, &nid))
                DebugLog(L"failed to show IP change notification");
        }
    } else {
        HICON icon = renderer_.RenderOffline();
        trayOnline_ = false;
        trayTooltip_ = L"Нет подключения";
        UpdateTrayIcon(icon, trayTooltip_);
    }
    refreshInProgress_.store(false);
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
            if (!timerId_)
                DebugLog(L"failed to recreate refresh timer");
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
        if (app->RestoreTrayIcon())
            app->StartRefreshThread();
        return 0;
    }

    switch (msg) {
    case WM_APP_TRAY:
        if (LOWORD(lp) == WM_RBUTTONUP || LOWORD(lp) == WM_CONTEXTMENU)
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
        {
            bool enable = !app->settings_.IsAutorunEnabled();
            if (app->settings_.SetAutorun(enable)) {
                app->settings_.startWithWindows = app->settings_.IsAutorunEnabled();
                app->settings_.Save();
            }
            break;
        }
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
