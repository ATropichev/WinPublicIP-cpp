#include <windows.h>
#include <commctrl.h>
#include <algorithm>
#include <string>
#include "SettingsDialog.h"

#pragma comment(lib, "comctl32.lib")

static const wchar_t* DLG_CLASS = L"WinPublicIPSettings";

struct DlgState {
    AppSettings* settings;
    bool         accepted = false;
    HWND         hEdit    = nullptr;
    HWND         hSpin    = nullptr;
    HFONT        hFont    = nullptr;
};

static void CenterWindow(HWND hWnd)
{
    RECT rc;
    GetWindowRect(hWnd, &rc);
    int w  = rc.right - rc.left;
    int h  = rc.bottom - rc.top;
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    SetWindowPos(hWnd, nullptr, (sw - w) / 2, (sh - h) / 2, 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER);
}

static LRESULT CALLBACK DlgProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp)
{
    auto* state = reinterpret_cast<DlgState*>(
        GetWindowLongPtrW(hWnd, GWLP_USERDATA));

    switch (msg) {
    case WM_CREATE: {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        state = reinterpret_cast<DlgState*>(cs->lpCreateParams);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));

        // Системный шрифт UI (Segoe UI, не жирный)
        NONCLIENTMETRICSW ncm{};
        ncm.cbSize = sizeof(ncm);
        SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
        state->hFont = CreateFontIndirectW(&ncm.lfMessageFont);

        auto setFont = [&](HWND h) {
            SendMessageW(h, WM_SETFONT, (WPARAM)state->hFont, TRUE);
        };

        // Метка — SS_CENTERIMAGE выравнивает текст по центру высоты
        HWND hLabel = CreateWindowExW(0, L"STATIC",
            L"Интервал обновления (сек):",
            WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE,
            12, 14, 188, 24, hWnd, nullptr, cs->hInstance, nullptr);

        // Edit — поле ввода (buddy для спиннера)
        state->hEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT",
            std::to_wstring(state->settings->refreshIntervalSeconds).c_str(),
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_NUMBER | ES_RIGHT,
            206, 14, 56, 22, hWnd,
            reinterpret_cast<HMENU>(100), cs->hInstance, nullptr);

        // Спиннер (стрелки вверх/вниз) — прикреплён к Edit как buddy
        state->hSpin = CreateWindowExW(0, UPDOWN_CLASSW, nullptr,
            WS_CHILD | WS_VISIBLE |
            UDS_SETBUDDYINT | UDS_ALIGNRIGHT | UDS_ARROWKEYS | UDS_HOTTRACK,
            0, 0, 0, 0, hWnd,
            reinterpret_cast<HMENU>(101), cs->hInstance, nullptr);
        SendMessageW(state->hSpin, UDM_SETBUDDY,   (WPARAM)state->hEdit, 0);
        SendMessageW(state->hSpin, UDM_SETRANGE32,  10, 3600);
        SendMessageW(state->hSpin, UDM_SETPOS32,    0,
                     state->settings->refreshIntervalSeconds);

        // Кнопки — по центру снизу с отступом 12px от края
        // Клиентская область 314px: (314 - 75 - 6 - 75) / 2 = 79
        HWND hOk = CreateWindowExW(0, L"BUTTON", L"OK",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
            79, 52, 75, 26, hWnd,
            reinterpret_cast<HMENU>(IDOK), cs->hInstance, nullptr);

        HWND hCancel = CreateWindowExW(0, L"BUTTON", L"Отмена",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            160, 52, 75, 26, hWnd,
            reinterpret_cast<HMENU>(IDCANCEL), cs->hInstance, nullptr);

        // Применяем шрифт ко всем элементам
        setFont(hWnd);
        setFont(hLabel);
        setFont(state->hEdit);
        setFont(state->hSpin);
        setFont(hOk);
        setFont(hCancel);

        CenterWindow(hWnd);
        SetFocus(state->hEdit);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wp) == IDOK) {
            BOOL ok = FALSE;
            int val = (int)SendMessageW(state->hSpin, UDM_GETPOS32, 0, (LPARAM)&ok);
            if (!ok) {
                wchar_t buf[32] = {};
                GetWindowTextW(state->hEdit, buf, _countof(buf));
                val = _wtoi(buf);
            }
            val = std::max(10, std::min(3600, val));
            state->settings->refreshIntervalSeconds = val;
            state->accepted = true;
            DestroyWindow(hWnd);
        } else if (LOWORD(wp) == IDCANCEL) {
            DestroyWindow(hWnd);
        }
        return 0;
    case WM_DESTROY:
        if (state->hFont) DeleteObject(state->hFont);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wp, lp);
}

bool ShowSettingsDialog(HWND hParent, HINSTANCE hInstance, AppSettings& settings)
{
    INITCOMMONCONTROLSEX icc{ sizeof(icc), ICC_UPDOWN_CLASS };
    InitCommonControlsEx(&icc);

    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = DlgProc;
    wc.hInstance     = hInstance;
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    wc.lpszClassName = DLG_CLASS;
    RegisterClassExW(&wc);

    DlgState state;
    state.settings = &settings;

    // Рассчитываем размер окна по нужной клиентской области (314 x 100)
    DWORD style   = WS_CAPTION | WS_SYSMENU;
    DWORD exStyle = WS_EX_DLGMODALFRAME | WS_EX_TOPMOST;
    RECT rc = {0, 0, 314, 100};
    AdjustWindowRectEx(&rc, style, FALSE, exStyle);

    CreateWindowExW(exStyle, DLG_CLASS,
        L"WinPublicIP — Настройки",
        WS_VISIBLE | style,
        0, 0, rc.right - rc.left, rc.bottom - rc.top,
        hParent, nullptr, hInstance, &state);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    UnregisterClassW(DLG_CLASS, hInstance);
    return state.accepted;
}
