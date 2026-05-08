#include <windows.h>
#include <shlobj.h>
#include <algorithm>
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

static void Normalize(AppSettings& s)
{
    AppSettings defaults;
    s.refreshIntervalSeconds = std::max(10, std::min(3600, s.refreshIntervalSeconds));
    if (s.ipProviders.empty())
        s.ipProviders = defaults.ipProviders;
    if (s.geoProvider.empty())
        s.geoProvider = defaults.geoProvider;
    if (s.vpnInterfacePatterns.empty())
        s.vpnInterfacePatterns = defaults.vpnInterfacePatterns;
}

static std::vector<std::string> ReadStringList(const json& value)
{
    std::vector<std::string> result;
    if (!value.is_array())
        return result;
    for (const auto& item : value) {
        if (item.is_string())
            result.push_back(item.get<std::string>());
    }
    return result;
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
                if (j.contains("refreshIntervalSeconds") && j["refreshIntervalSeconds"].is_number_integer())
                    s.refreshIntervalSeconds = j["refreshIntervalSeconds"];
                if (j.contains("geoProvider") && j["geoProvider"].is_string())
                    s.geoProvider = j["geoProvider"];
                if (j.contains("notifyOnIpChange") && j["notifyOnIpChange"].is_boolean())
                    s.notifyOnIpChange = j["notifyOnIpChange"];
                if (j.contains("startWithWindows") && j["startWithWindows"].is_boolean())
                    s.startWithWindows = j["startWithWindows"];
                if (j.contains("ipProviders"))
                    s.ipProviders = ReadStringList(j["ipProviders"]);
                if (j.contains("vpnInterfacePatterns"))
                    s.vpnInterfacePatterns = ReadStringList(j["vpnInterfacePatterns"]);
            }
        }
    } catch (...) {}
    Normalize(s);
    return s;
}

void AppSettings::Save() const
{
    try {
        auto path = ConfigPath();
        fs::create_directories(path.parent_path());
        json j;
        j["refreshIntervalSeconds"] = refreshIntervalSeconds;
        j["geoProvider"]            = geoProvider;
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
