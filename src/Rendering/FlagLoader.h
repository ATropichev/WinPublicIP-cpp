#pragma once
#include <gdiplus.h>
#include <string>
#include <unordered_map>
#include <memory>

class FlagLoader {
public:
    // Возвращает Bitmap для кода страны (нижний регистр). Никогда не nullptr.
    Gdiplus::Bitmap* Load(const std::string& countryCode);
private:
    std::unordered_map<std::string, std::unique_ptr<Gdiplus::Bitmap>> cache_;
    std::unique_ptr<Gdiplus::Bitmap> LoadFromResource(const std::string& code);
    std::unique_ptr<Gdiplus::Bitmap> MakeFallback();
};
