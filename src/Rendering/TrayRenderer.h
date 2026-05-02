#pragma once
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
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

    HICON Compose(Gdiplus::Bitmap* background, Gdiplus::Bitmap* overlay);
    std::unique_ptr<Gdiplus::Bitmap> shieldOn_;
    std::unique_ptr<Gdiplus::Bitmap> shieldOff_;
    std::unique_ptr<Gdiplus::Bitmap> offline_;
};
