#include "IpProvider.h"
#include "HttpHelper.h"
#include <nlohmann/json.hpp>
#include <stdexcept>

using json = nlohmann::json;

IpProvider::IpProvider(const std::vector<std::string>& providers)
    : providers_(providers) {}

std::string IpProvider::Get()
{
    for (const auto& url : providers_) {
        try {
            std::string body = HttpGet(url);
            // Trim
            while (!body.empty() && (body.back() == '\n' || body.back() == '\r' || body.back() == ' '))
                body.pop_back();
            // JSON {"ip":"..."} или plain text
            if (!body.empty() && body.front() == '{') {
                auto j = json::parse(body, nullptr, false);
                if (!j.is_discarded() && j.contains("ip"))
                    return j["ip"].get<std::string>();
            }
            if (!body.empty()) return body;
        } catch (...) {}
    }
    throw std::runtime_error("All IP providers unavailable");
}
