#pragma once
#include <gdiplus.h>
#include <windows.h>
#include <string>
#include "FlagLoader.h"

class TrayRenderer {
public:
    TrayRenderer();
    ~TrayRenderer();

    HICON Render(const std::string& countryCode, bool vpnOn);
    HICON RenderOffline();

private:
    FlagLoader flagLoader_;
    HICON      lastIcon_ = nullptr;

    HICON Compose(Gdiplus::Bitmap* background, const std::string& overlayRes);
    Gdiplus::Bitmap* LoadResource(const std::string& name);
    std::unique_ptr<Gdiplus::Bitmap> shieldOn_;
    std::unique_ptr<Gdiplus::Bitmap> shieldOff_;
    std::unique_ptr<Gdiplus::Bitmap> offline_;
};
