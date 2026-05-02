#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <fstream>
#include <filesystem>
#include "AppSettings.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;
namespace fs = std::filesystem;

static fs::path ConfigPath()
{
    wchar_t* appData = nullptr;
    SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &appData);
    fs::path p = fs::path(appData) / L"WinPublicIP" / L"config.json";
    CoTaskMemFree(appData);
    return p;
}

AppSettings AppSettings::Load()
{
    AppSettings s;
    try {
        auto path = ConfigPath();
        if (fs::exists(path)) {
            std::ifstream f(path);
            auto j = json::parse(f, nullptr, false);
            if (!j.is_discarded()) {
                if (j.contains("refreshIntervalSeconds")) s.refreshIntervalSeconds = j["refreshIntervalSeconds"];
                if (j.contains("homeCountry"))            s.homeCountry            = j["homeCountry"];
                if (j.contains("notifyOnIpChange"))       s.notifyOnIpChange       = j["notifyOnIpChange"];
                if (j.contains("startWithWindows"))       s.startWithWindows       = j["startWithWindows"];
                if (j.contains("ipProviders"))            s.ipProviders            = j["ipProviders"].get<std::vector<std::string>>();
                if (j.contains("vpnInterfacePatterns"))   s.vpnInterfacePatterns   = j["vpnInterfacePatterns"].get<std::vector<std::string>>();
            }
        }
    } catch (...) {}
    return s;
}

void AppSettings::Save() const
{
    try {
        auto path = ConfigPath();
        fs::create_directories(path.parent_path());
        json j;
        j["refreshIntervalSeconds"] = refreshIntervalSeconds;
        j["homeCountry"]            = homeCountry;
        j["ipProviders"]            = ipProviders;
        j["vpnInterfacePatterns"]   = vpnInterfacePatterns;
        j["notifyOnIpChange"]       = notifyOnIpChange;
        j["startWithWindows"]       = startWithWindows;
        std::ofstream f(path);
        f << j.dump(2);
    } catch (...) {}
}

void AppSettings::SetAutorun(bool enable) const
{
    const wchar_t* keyPath = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    const wchar_t* valName = L"WinPublicIP";
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, keyPath, 0, KEY_SET_VALUE, &hKey) != ERROR_SUCCESS)
        return;
    if (enable) {
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        std::wstring val = std::wstring(L"\"") + exePath + L"\"";
        RegSetValueExW(hKey, valName, 0, REG_SZ,
            reinterpret_cast<const BYTE*>(val.c_str()),
            static_cast<DWORD>((val.size() + 1) * sizeof(wchar_t)));
    } else {
        RegDeleteValueW(hKey, valName);
    }
    RegCloseKey(hKey);
}
