#pragma once
#include <string>
#include <unordered_map>

struct GeoResult {
    std::string country;
    std::string countryCode; // нижний регистр, ISO 3166-1 alpha-2
    std::string isp;
};

class GeoProvider {
public:
    explicit GeoProvider(std::string providerTemplate =
        "http://ip-api.com/json/{ip}?fields=status,message,country,countryCode,isp,query");
    GeoResult Get(const std::string& ip);
    const GeoResult* LastResult() const;
private:
    bool usesDefaultProvider_ = true;
    std::string providerTemplate_;
    std::unordered_map<std::string, GeoResult> cache_;
    GeoResult last_{};
    bool hasLast_ = false;
};
