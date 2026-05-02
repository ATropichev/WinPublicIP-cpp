#include "GeoProvider.h"
#include "HttpHelper.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <stdexcept>

using json = nlohmann::json;

GeoResult GeoProvider::Get(const std::string& ip)
{
    auto it = cache_.find(ip);
    if (it != cache_.end()) {
        last_ = it->second;
        hasLast_ = true;
        return last_;
    }

    std::string url = "http://ip-api.com/json/" + ip +
                      "?fields=country,countryCode,isp,query";
    std::string body = HttpGet(url);
    auto j = json::parse(body, nullptr, false);
    if (j.is_discarded()) throw std::runtime_error("GeoProvider: bad JSON");

    GeoResult r;
    r.country     = j.value("country",     "Unknown");
    r.countryCode = j.value("countryCode", "xx");
    r.isp         = j.value("isp",         "");
    // Приводим код страны к нижнему регистру
    std::transform(r.countryCode.begin(), r.countryCode.end(),
                   r.countryCode.begin(), ::tolower);

    cache_[ip] = r;
    last_      = r;
    hasLast_   = true;
    return r;
}

const GeoResult* GeoProvider::LastResult() const
{
    return hasLast_ ? &last_ : nullptr;
}
