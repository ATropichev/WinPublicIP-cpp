#pragma once
#include <string>
#include <vector>

struct AppSettings {
    int  refreshIntervalSeconds  = 60;
    std::vector<std::string> ipProviders = {
        "https://api.ipify.org?format=json",
        "https://ifconfig.me/ip",
        "https://icanhazip.com"
    };
    std::string geoProvider =
        "http://ip-api.com/json/{ip}?fields=status,message,country,countryCode,isp,query";
    std::vector<std::string> vpnInterfacePatterns = {
        "TAP","tun","WireGuard","OpenVPN",
        "Tailscale","ZeroTier","PPP","WAN Miniport (IKEv2)"
    };
    bool notifyOnIpChange = true;
    bool startWithWindows = false;

    static AppSettings Load();
    void Save() const;
    bool IsAutorunEnabled() const;
    bool SetAutorun(bool enable) const;
};
