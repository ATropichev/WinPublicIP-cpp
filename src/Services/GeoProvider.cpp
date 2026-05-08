#include "GeoProvider.h"
#include "HttpHelper.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <utility>

using json = nlohmann::json;

GeoProvider::GeoProvider(std::string providerTemplate)
    : providerTemplate_(std::move(providerTemplate)) {}

GeoResult GeoProvider::Get(const std::string& ip)
{
    auto it = cache_.find(ip);
    if (it != cache_.end()) {
        last_ = it->second;
        hasLast_ = true;
        return last_;
    }

    std::string url = providerTemplate_;
    size_t marker = url.find("{ip}");
    if (marker != std::string::npos)
        url.replace(marker, 4, ip);
    else
        url += ip;
    std::string body = HttpGet(url);
    auto j = json::parse(body, nullptr, false);
    if (j.is_discarded()) throw std::runtime_error("GeoProvider: bad JSON");

    GeoResult r;
    r.country     = j.value("country",     "Unknown");
    r.countryCode = j.value("countryCode", "xx");
    r.isp         = j.value("isp",         "");
    // Приводим код страны к нижнему регистру
    std::transform(r.countryCode.begin(), r.countryCode.end(),
                   r.countryCode.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    cache_[ip] = r;
    last_      = r;
    hasLast_   = true;
    return r;
}

const GeoResult* GeoProvider::LastResult() const
{
    return hasLast_ ? &last_ : nullptr;
}
