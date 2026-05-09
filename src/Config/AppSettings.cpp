#include <windows.h>
#include <shlobj.h>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <cwchar>
#include <cwctype>
#include <vector>
#include "AppSettings.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;
namespace fs = std::filesystem;

enum class BackupResult {
    Failed,
    Moved,
    Copied,
};

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

static const wchar_t* AutorunKeyPath()
{
    return L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
}

static const wchar_t* AutorunValueName()
{
    return L"WinPublicIP";
}

static std::wstring CurrentExePath()
{
    std::vector<wchar_t> exePath(MAX_PATH);
    for (;;) {
        DWORD len = GetModuleFileNameW(nullptr, exePath.data(),
            static_cast<DWORD>(exePath.size()));
        if (len == 0)
            return {};
        if (len < exePath.size() - 1)
            return std::wstring(exePath.data(), len);
        if (exePath.size() >= 32768)
            return {};
        exePath.resize(exePath.size() * 2);
    }
}

static std::wstring FullPathOrOriginal(const std::wstring& path)
{
    DWORD needed = GetFullPathNameW(path.c_str(), 0, nullptr, nullptr);
    if (needed == 0)
        return path;
    std::vector<wchar_t> full(needed);
    DWORD len = GetFullPathNameW(path.c_str(), static_cast<DWORD>(full.size()),
        full.data(), nullptr);
    if (len == 0 || len >= full.size())
        return path;
    return std::wstring(full.data(), len);
}

static bool IsRegularFilePath(const std::wstring& path)
{
    std::error_code ec;
    return fs::is_regular_file(fs::path(path), ec);
}

static size_t FindExeExtension(const std::wstring& command, size_t offset)
{
    std::wstring lower = command;
    std::transform(lower.begin(), lower.end(), lower.begin(),
        [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
    return lower.find(L".exe", offset);
}

static std::wstring ExtractExecutablePath(const std::wstring& command)
{
    if (command.empty())
        return {};
    if (command.front() == L'"') {
        size_t end = command.find(L'"', 1);
        if (end != std::wstring::npos)
            return command.substr(1, end - 1);
    }
    size_t offset = 0;
    while (true) {
        size_t exe = FindExeExtension(command, offset);
        if (exe == std::wstring::npos)
            break;
        std::wstring candidate = command.substr(0, exe + 4);
        if (IsRegularFilePath(candidate))
            return candidate;
        offset = exe + 4;
    }
    size_t end = command.find_first_of(L" \t");
    return end == std::wstring::npos ? command : command.substr(0, end);
}

static bool SamePath(const std::wstring& a, const std::wstring& b)
{
    if (a.empty() || b.empty())
        return false;
    std::wstring left = FullPathOrOriginal(a);
    std::wstring right = FullPathOrOriginal(b);
    return _wcsicmp(left.c_str(), right.c_str()) == 0;
}

static bool ReadAutorunCommand(std::wstring& command)
{
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, AutorunKeyPath(), 0, KEY_QUERY_VALUE, &hKey) != ERROR_SUCCESS)
        return false;

    DWORD type = 0;
    DWORD bytes = 0;
    LONG rc = RegQueryValueExW(hKey, AutorunValueName(), nullptr, &type, nullptr, &bytes);
    if (rc != ERROR_SUCCESS || bytes == 0 || (type != REG_SZ && type != REG_EXPAND_SZ)) {
        RegCloseKey(hKey);
        return false;
    }

    std::vector<wchar_t> value(bytes / sizeof(wchar_t) + 1, L'\0');
    rc = RegQueryValueExW(hKey, AutorunValueName(), nullptr, &type,
        reinterpret_cast<BYTE*>(value.data()), &bytes);
    RegCloseKey(hKey);

    if (rc != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ))
        return false;
    if (type == REG_EXPAND_SZ) {
        DWORD needed = ExpandEnvironmentStringsW(value.data(), nullptr, 0);
        if (needed > 0) {
            std::vector<wchar_t> expanded(needed);
            DWORD len = ExpandEnvironmentStringsW(value.data(), expanded.data(),
                static_cast<DWORD>(expanded.size()));
            if (len > 0 && len <= expanded.size())
                command.assign(expanded.data());
            else
                command.assign(value.data());
        }
        else
            command.assign(value.data());
    } else {
        command.assign(value.data());
    }
    return true;
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

static BackupResult BackupConfig(const fs::path& path)
{
    std::error_code ec;
    if (path.empty() || !fs::exists(path, ec))
        return BackupResult::Failed;

    for (int i = 0; i < 100; ++i) {
        fs::path backup = InvalidConfigPath(path, i);
        if (fs::exists(backup, ec))
            continue;

        fs::rename(path, backup, ec);
        if (!ec)
            return BackupResult::Moved;

        ec.clear();
        if (fs::copy_file(path, backup, fs::copy_options::none, ec)) {
            auto originalSize = fs::file_size(path, ec);
            if (ec) continue;
            auto backupSize = fs::file_size(backup, ec);
            if (!ec && originalSize == backupSize)
                return BackupResult::Copied;
        }
    }

    DebugLog(L"failed to create config backup");
    return BackupResult::Failed;
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
    BackupResult backupResult = BackupResult::Failed;
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
                if (j.contains("ipProviders"))
                    s.ipProviders = ReadStringList(j["ipProviders"]);
                if (j.contains("vpnInterfacePatterns"))
                    s.vpnInterfacePatterns = ReadStringList(j["vpnInterfacePatterns"]);
            } else {
                backupResult = BackupConfig(path);
                shouldSave = backupResult != BackupResult::Failed;
                if (!shouldSave)
                    DebugLog(L"skipped repairing malformed config because backup failed");
            }
        }
    } catch (...) {
        DebugLog(L"failed to load config");
    }
    bool normalized = Normalize(s);
    if (normalized && !shouldSave) {
        backupResult = BackupConfig(path);
        shouldSave = backupResult != BackupResult::Failed;
        if (!shouldSave)
            DebugLog(L"skipped saving normalized config because backup failed");
    }
    s.startWithWindows = s.IsAutorunEnabled();
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

bool AppSettings::IsAutorunEnabled() const
{
    std::wstring command;
    if (!ReadAutorunCommand(command))
        return false;
    return SamePath(ExtractExecutablePath(command), CurrentExePath());
}

bool AppSettings::SetAutorun(bool enable) const
{
    HKEY hKey;
    DWORD disposition = 0;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, AutorunKeyPath(), 0, nullptr, 0,
            KEY_SET_VALUE, nullptr, &hKey, &disposition) != ERROR_SUCCESS) {
        DebugLog(L"failed to open or create autorun registry key");
        return false;
    }
    bool ok = true;
    if (enable) {
        std::wstring exePath = CurrentExePath();
        if (exePath.empty()) {
            RegCloseKey(hKey);
            DebugLog(L"failed to get executable path for autorun");
            return false;
        }
        std::wstring val = std::wstring(L"\"") + exePath + L"\"";
        if (RegSetValueExW(hKey, AutorunValueName(), 0, REG_SZ,
            reinterpret_cast<const BYTE*>(val.c_str()),
            static_cast<DWORD>((val.size() + 1) * sizeof(wchar_t))) != ERROR_SUCCESS) {
            DebugLog(L"failed to set autorun registry value");
            ok = false;
        }
    } else {
        LONG rc = RegDeleteValueW(hKey, AutorunValueName());
        if (rc != ERROR_SUCCESS && rc != ERROR_FILE_NOT_FOUND) {
            DebugLog(L"failed to delete autorun registry value");
            ok = false;
        }
    }
    RegCloseKey(hKey);
    return ok;
}
