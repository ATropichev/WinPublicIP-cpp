#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <algorithm>
#include <string>
#include <fstream>
#include <vector>
#include "VpnDetector.h"

static void VpnLog(const std::string& msg)
{
    char tmp[MAX_PATH];
    GetTempPathA(MAX_PATH, tmp);
    std::ofstream f(std::string(tmp) + "WinPublicIP.log", std::ios::app);
    f << "[VPN] " << msg << "\n";
}

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

static std::wstring ToWide(const std::string& s)
{
    if (s.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w(len, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), w.data(), len);
    return w;
}

static bool ContainsCI(const std::wstring& haystack, const std::wstring& needle)
{
    auto it = std::search(
        haystack.begin(), haystack.end(),
        needle.begin(),   needle.end(),
        [](wchar_t a, wchar_t b){ return towlower(a) == towlower(b); });
    return it != haystack.end();
}

VpnDetector::VpnDetector(const std::vector<std::string>& patterns)
{
    for (const auto& p : patterns)
        patterns_.push_back(ToWide(p));
}

bool VpnDetector::IsActive() const
{
    ULONG bufLen = 0;
    GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_ALL_INTERFACES,
                         nullptr, nullptr, &bufLen);
    std::vector<BYTE> buf(bufLen);
    auto* adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buf.data());
    if (GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_ALL_INTERFACES,
                             nullptr, adapters, &bufLen) != ERROR_SUCCESS)
        return false;

    for (auto* a = adapters; a; a = a->Next) {
        // FriendlyName и Description — оба PWSTR в IP_ADAPTER_ADDRESSES
        std::wstring name(a->FriendlyName ? a->FriendlyName : L"");
        std::wstring desc = name;
        if (a->Description)
            desc += L" " + std::wstring(a->Description);

        // Лог: имя интерфейса и статус
        std::string nameA(name.begin(), name.end());
        VpnLog("Adapter [status=" + std::to_string(a->OperStatus) + "]: " + nameA);

        if (a->OperStatus != IfOperStatusUp) continue;

        for (const auto& pattern : patterns_) {
            if (ContainsCI(desc, pattern)) {
                std::string pat(pattern.begin(), pattern.end());
                VpnLog("  -> MATCH pattern: " + pat);
                return true;
            }
        }
    }
    return false;
}
