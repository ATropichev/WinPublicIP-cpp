#include <windows.h>
#include <winhttp.h>
#include <stdexcept>
#include <string>
#include <vector>
#include "HttpHelper.h"

#pragma comment(lib, "winhttp.lib")

// RAII-обёртка для HINTERNET — гарантирует закрытие при любом выходе из функции
struct WinHttpHandle {
    HINTERNET h = nullptr;
    explicit WinHttpHandle(HINTERNET handle = nullptr) : h(handle) {}
    ~WinHttpHandle() { if (h) WinHttpCloseHandle(h); }
    WinHttpHandle(const WinHttpHandle&)            = delete;
    WinHttpHandle& operator=(const WinHttpHandle&) = delete;
    operator HINTERNET() const { return h; }
    explicit operator bool() const { return h != nullptr; }
};

static std::wstring ToWide(const std::string& s)
{
    if (s.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w(len, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), w.data(), len);
    return w;
}

std::string HttpGet(const std::string& url, int timeoutMs)
{
    std::wstring wUrl = ToWide(url);

    URL_COMPONENTS uc{};
    uc.dwStructSize     = sizeof(uc);
    wchar_t host[256]   = {};
    wchar_t path[1024]  = {};
    wchar_t extra[1024] = {};
    uc.lpszHostName     = host;
    uc.dwHostNameLength = _countof(host);
    uc.lpszUrlPath      = path;
    uc.dwUrlPathLength  = _countof(path);
    uc.lpszExtraInfo    = extra;
    uc.dwExtraInfoLength = _countof(extra);
    if (!WinHttpCrackUrl(wUrl.c_str(), 0, 0, &uc))
        throw std::runtime_error("HttpGet: bad URL");

    bool isHttps = (uc.nScheme == INTERNET_SCHEME_HTTPS);
    std::wstring objectName = uc.dwUrlPathLength > 0
        ? std::wstring(path, uc.dwUrlPathLength)
        : L"/";
    if (uc.dwExtraInfoLength > 0)
        objectName.append(extra, uc.dwExtraInfoLength);

    WinHttpHandle hSession(WinHttpOpen(L"WinPublicIP/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, nullptr, nullptr, 0));
    if (!hSession) throw std::runtime_error("WinHttpOpen failed");

    WinHttpHandle hConnect(WinHttpConnect(hSession, host, uc.nPort, 0));
    if (!hConnect) throw std::runtime_error("WinHttpConnect failed");

    DWORD flags = isHttps ? WINHTTP_FLAG_SECURE : 0;
    WinHttpHandle hRequest(WinHttpOpenRequest(hConnect, L"GET", objectName.c_str(),
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags));
    if (!hRequest) throw std::runtime_error("WinHttpOpenRequest failed");

    DWORD timeout = static_cast<DWORD>(timeoutMs);
    WinHttpSetOption(hRequest, WINHTTP_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
    WinHttpSetOption(hRequest, WINHTTP_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));
    WinHttpSetOption(hRequest, WINHTTP_OPTION_SEND_TIMEOUT,    &timeout, sizeof(timeout));
    WinHttpSetOption(hRequest, WINHTTP_OPTION_RESOLVE_TIMEOUT, &timeout, sizeof(timeout));

    bool ok = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                 WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
              WinHttpReceiveResponse(hRequest, nullptr);

    if (!ok) throw std::runtime_error("HttpGet: request failed");

    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    if (!WinHttpQueryHeaders(hRequest,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize,
            WINHTTP_NO_HEADER_INDEX)) {
        throw std::runtime_error("HttpGet: status unavailable");
    }
    if (statusCode < 200 || statusCode >= 300)
        throw std::runtime_error("HttpGet: non-success status");

    std::string body;
    DWORD avail = 0;
    while (WinHttpQueryDataAvailable(hRequest, &avail) && avail > 0) {
        std::vector<char> buf(avail);
        DWORD read = 0;
        if (!WinHttpReadData(hRequest, buf.data(), avail, &read))
            throw std::runtime_error("HttpGet: read failed");
        body.append(buf.data(), read);
    }
    // hRequest, hConnect, hSession закрываются деструкторами RAII-обёрток
    return body;
}
