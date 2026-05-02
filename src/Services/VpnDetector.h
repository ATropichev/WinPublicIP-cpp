#pragma once
#include <vector>
#include <string>

class VpnDetector {
public:
    explicit VpnDetector(const std::vector<std::string>& patterns);
    bool IsActive() const;
private:
    std::vector<std::wstring> patterns_;
};
