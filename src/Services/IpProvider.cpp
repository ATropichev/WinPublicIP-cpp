#include "IpProvider.h"
#include "HttpHelper.h"
#include <nlohmann/json.hpp>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdexcept>

using json = nlohmann::json;

static void DebugLog(const std::string& message)
{
    OutputDebugStringA(("WinPublicIP: " + message + "\n").c_str());
}

static bool IsValidIp(const std::string& value)
{
    IN_ADDR ipv4{};
    IN6_ADDR ipv6{};
    return InetPtonA(AF_INET, value.c_str(), &ipv4) == 1 ||
           InetPtonA(AF_INET6, value.c_str(), &ipv6) == 1;
}

static void Trim(std::string& value)
{
    while (!value.empty() && (value.back() == '\n' || value.back() == '\r' ||
           value.back() == ' ' || value.back() == '\t'))
        value.pop_back();
    while (!value.empty() && (value.front() == '\n' || value.front() == '\r' ||
           value.front() == ' ' || value.front() == '\t'))
        value.erase(value.begin());
}

IpProvider::IpProvider(const std::vector<std::string>& providers)
    : providers_(providers) {}

std::string IpProvider::Get()
{
    for (const auto& url : providers_) {
        try {
            std::string body = HttpGet(url);
            Trim(body);
            // JSON {"ip":"..."} или plain text
            if (!body.empty() && body.front() == '{') {
                auto j = json::parse(body, nullptr, false);
                if (!j.is_discarded() && j.contains("ip")) {
                    std::string ip = j["ip"].get<std::string>();
                    Trim(ip);
                    if (IsValidIp(ip))
                        return ip;
                }
            }
            if (IsValidIp(body)) return body;
        } catch (const std::exception& e) {
            DebugLog("IP provider failed: " + url + " (" + e.what() + ")");
        } catch (...) {
            DebugLog("IP provider failed: " + url);
        }
    }
    throw std::runtime_error("All IP providers unavailable");
}
