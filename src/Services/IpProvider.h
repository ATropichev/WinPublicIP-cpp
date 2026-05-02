#pragma once
#include <string>
#include <vector>

class IpProvider {
public:
    explicit IpProvider(const std::vector<std::string>& providers);
    std::string Get(); // возвращает IP или бросает std::runtime_error
private:
    std::vector<std::string> providers_;
};
