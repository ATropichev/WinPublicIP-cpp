#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <string>
#include "SettingsDialog.h"

static const wchar_t* DLG_CLASS = L"WinPublicIPSettings";

struct DlgState {
    AppSettings* settings;
    bool         accepted = false;
    HWND         hEdit    = nullptr;
};

static void CenterWindow(HWND hWnd)
{
    RECT rc;
    GetWindowRect(hWnd, &rc);
    int w = rc.right  - rc.left;
    int h = rc.bottom - rc.top;
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    SetWindowPos(hWnd, nullptr,
        (sw - w) / 2, (sh - h) / 2, 0, 0,
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

        // Label
        CreateWindowExW(0, L"STATIC",
            L"Интервал обновления (сек):",
            WS_CHILD | WS_VISIBLE,
            12, 16, 190, 20, hWnd, nullptr, cs->hInstance, nullptr);

        // Edit
        state->hEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT",
            std::to_wstring(state->settings->refreshIntervalSeconds).c_str(),
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_NUMBER,
            210, 12, 80, 24, hWnd,
            reinterpret_cast<HMENU>(100), cs->hInstance, nullptr);

        // OK
        CreateWindowExW(0, L"BUTTON", L"OK",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
            152, 52, 68, 26, hWnd,
            reinterpret_cast<HMENU>(IDOK), cs->hInstance, nullptr);

        // Отмена
        CreateWindowExW(0, L"BUTTON", L"Отмена",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            228, 52, 68, 26, hWnd,
            reinterpret_cast<HMENU>(IDCANCEL), cs->hInstance, nullptr);

        CenterWindow(hWnd);
        SetFocus(state->hEdit);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wp) == IDOK) {
            wchar_t buf[32] = {};
            GetWindowTextW(state->hEdit, buf, _countof(buf));
            int val = _wtoi(buf);
            if (val < 10)   val = 10;
            if (val > 3600) val = 3600;
            state->settings->refreshIntervalSeconds = val;
            state->accepted = true;
            DestroyWindow(hWnd);
        } else if (LOWORD(wp) == IDCANCEL) {
            DestroyWindow(hWnd);
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wp, lp);
}

bool ShowSettingsDialog(HWND hParent, HINSTANCE hInstance, AppSettings& settings)
{
    // Регистрируем класс окна (один раз)
    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = DlgProc;
    wc.hInstance     = hInstance;
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    wc.lpszClassName = DLG_CLASS;
    RegisterClassExW(&wc);

    DlgState state;
    state.settings = &settings;

    HWND hDlg = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        DLG_CLASS,
        L"WinPublicIP — Настройки",
        WS_VISIBLE | WS_CAPTION | WS_SYSMENU,
        0, 0, 320, 110,
        hParent, nullptr, hInstance, &state);

    // Локальный message loop — блокируем до закрытия диалога
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    UnregisterClassW(DLG_CLASS, hInstance);
    return state.accepted;
}
