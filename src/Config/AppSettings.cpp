#include <windows.h>
#include <shlobj.h>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include "AppSettings.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;
namespace fs = std::filesystem;

static void DebugLog(const wchar_t* message)
{
    OutputDebugStringW(L"WinPublicIP: ");
    OutputDebugStringW(message);
    OutputDebugStringW(L"\n");
}

static fs::path ConfigPath()
{
    wchar_t* appData = nullptr;
    if (SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &appData) != S_OK)
        return {};
    fs::path p = fs::path(appData) / L"WinPublicIP" / L"config.json";
    CoTaskMemFree(appData);
    return p;
}

static fs::path InvalidConfigPath(const fs::path& path, int counter)
{
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    std::tm tm{};
    localtime_s(&tm, &t);

    std::wostringstream suffix;
    suffix << L".invalid." << std::put_time(&tm, L"%Y%m%d-%H%M%S")
           << L"-" << std::setw(3) << std::setfill(L'0') << ms.count();
    if (counter > 0)
        suffix << L"." << counter;
    suffix << L".json";
    return path.parent_path() / (path.stem().wstring() + suffix.str());
}

static bool BackupConfig(const fs::path& path)
{
    std::error_code ec;
    if (path.empty() || !fs::exists(path, ec))
        return false;

    for (int i = 0; i < 100; ++i) {
        fs::path backup = InvalidConfigPath(path, i);
        if (fs::exists(backup, ec))
            continue;

        fs::rename(path, backup, ec);
        if (!ec)
            return true;

        ec.clear();
        if (fs::copy_file(path, backup, fs::copy_options::none, ec))
            return true;
    }

    DebugLog(L"failed to create config backup");
    return false;
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
                shouldSave = BackupConfig(path);
                if (!shouldSave)
                    DebugLog(L"skipped repairing malformed config because backup failed");
            }
        }
    } catch (...) {
        DebugLog(L"failed to load config");
    }
    bool normalized = Normalize(s);
    if (normalized && !shouldSave) {
        shouldSave = BackupConfig(path);
        if (!shouldSave)
            DebugLog(L"skipped saving normalized config because backup failed");
    }
    if (shouldSave)
        s.Save();
    return s;
}

void AppSettings::Save() const
{
    try {
        auto path = ConfigPath();
        if (path.empty())
            return;
        fs::create_directories(path.parent_path());
        json j;
        j["refreshIntervalSeconds"] = refreshIntervalSeconds;
        j["geoProvider"]            = geoProvider;
        j["ipProviders"]            = ipProviders;
        j["vpnInterfacePatterns"]   = vpnInterfacePatterns;
        j["notifyOnIpChange"]       = notifyOnIpChange;
        j["startWithWindows"]       = startWithWindows;
        fs::path tempPath = path;
        tempPath += L".tmp";
        std::ofstream f(tempPath, std::ios::trunc);
        f << j.dump(2);
        f.close();
        if (!f)
            throw std::runtime_error("config write failed");
        if (!MoveFileExW(tempPath.c_str(), path.c_str(),
                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            std::error_code ec;
            fs::remove(tempPath, ec);
            throw std::runtime_error("config replace failed");
        }
    } catch (...) {
        DebugLog(L"failed to save config");
    }
}

void AppSettings::SetAutorun(bool enable) const
{
    const wchar_t* keyPath = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    const wchar_t* valName = L"WinPublicIP";
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, keyPath, 0, KEY_SET_VALUE, &hKey) != ERROR_SUCCESS) {
        DebugLog(L"failed to open autorun registry key");
        return;
    }
    if (enable) {
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        std::wstring val = std::wstring(L"\"") + exePath + L"\"";
        if (RegSetValueExW(hKey, valName, 0, REG_SZ,
            reinterpret_cast<const BYTE*>(val.c_str()),
            static_cast<DWORD>((val.size() + 1) * sizeof(wchar_t))) != ERROR_SUCCESS) {
            DebugLog(L"failed to set autorun registry value");
        }
    } else {
        LONG rc = RegDeleteValueW(hKey, valName);
        if (rc != ERROR_SUCCESS && rc != ERROR_FILE_NOT_FOUND)
            DebugLog(L"failed to delete autorun registry value");
    }
    RegCloseKey(hKey);
}
