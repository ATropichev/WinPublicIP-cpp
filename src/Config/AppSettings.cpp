#include <windows.h>
#include <shlobj.h>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <sstream>
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

static fs::path InvalidConfigPath(const fs::path& path)
{
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_s(&tm, &t);

    std::wostringstream suffix;
    suffix << L".invalid." << std::put_time(&tm, L"%Y%m%d-%H%M%S") << L".json";
    return path.parent_path() / (path.stem().wstring() + suffix.str());
}

static void BackupConfig(const fs::path& path)
{
    if (!fs::exists(path))
        return;
    fs::path backup = InvalidConfigPath(path);
    std::error_code ec;
    fs::rename(path, backup, ec);
    if (ec) {
        fs::copy_file(path, backup, fs::copy_options::overwrite_existing, ec);
    }
}

static bool Normalize(AppSettings& s)
{
    bool changed = false;
    AppSettings defaults;
    int interval = std::max(10, std::min(3600, s.refreshIntervalSeconds));
    if (interval != s.refreshIntervalSeconds) {
        s.refreshIntervalSeconds = interval;
        changed = true;
    }
    if (s.ipProviders.empty()) {
        s.ipProviders = defaults.ipProviders;
        changed = true;
    }
    if (s.geoProvider.empty()) {
        s.geoProvider = defaults.geoProvider;
        changed = true;
    }
    if (s.vpnInterfacePatterns.empty()) {
        s.vpnInterfacePatterns = defaults.vpnInterfacePatterns;
        changed = true;
    }
    return changed;
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
    bool shouldSave = false;
    fs::path path;
    try {
        path = ConfigPath();
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
            } else {
                BackupConfig(path);
                shouldSave = true;
            }
        }
    } catch (...) {}
    bool normalized = Normalize(s);
    if (normalized && !shouldSave)
        BackupConfig(path);
    shouldSave = normalized || shouldSave;
    if (shouldSave)
        s.Save();
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
