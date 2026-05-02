#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <gdiplus.h>
#include <algorithm>
#include <cctype>
#include "FlagLoader.h"

static std::string ToUpper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::toupper);
    return s;
}

// Загружает RCDATA-ресурс в IStream
static IStream* ResourceToStream(const std::string& name)
{
    HMODULE hMod = GetModuleHandleW(nullptr);
    std::string resName = "FLAG_" + ToUpper(name);
    HRSRC hRes = FindResourceA(hMod, resName.c_str(), "RCDATA");
    if (!hRes) return nullptr;
    HGLOBAL hGlob = LoadResource(hMod, hRes);
    if (!hGlob) return nullptr;
    DWORD size = SizeofResource(hMod, hRes);
    void* data = LockResource(hGlob);
    if (!data) return nullptr;

    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, size);
    if (!hMem) return nullptr;
    void* buf = GlobalLock(hMem);
    if (!buf) { GlobalFree(hMem); return nullptr; }
    memcpy(buf, data, size);
    GlobalUnlock(hMem);

    IStream* stream = nullptr;
    CreateStreamOnHGlobal(hMem, TRUE, &stream);
    return stream;
}

std::unique_ptr<Gdiplus::Bitmap> FlagLoader::LoadFromResource(const std::string& code)
{
    IStream* stream = ResourceToStream(code);
    if (!stream) return nullptr;
    auto bmp = std::make_unique<Gdiplus::Bitmap>(stream);
    stream->Release();
    if (bmp->GetLastStatus() != Gdiplus::Ok) return nullptr;
    return bmp;
}

std::unique_ptr<Gdiplus::Bitmap> FlagLoader::MakeFallback()
{
    auto bmp = std::make_unique<Gdiplus::Bitmap>(32, 32, PixelFormat32bppARGB);
    Gdiplus::Graphics g(bmp.get());
    Gdiplus::SolidBrush br(Gdiplus::Color(255, 100, 100, 100));
    g.FillRectangle(&br, 0, 0, 32, 32);
    return bmp;
}

Gdiplus::Bitmap* FlagLoader::Load(const std::string& countryCode)
{
    auto it = cache_.find(countryCode);
    if (it != cache_.end()) return it->second.get();

    auto bmp = LoadFromResource(countryCode);
    if (!bmp) bmp = LoadFromResource("xx");
    if (!bmp) bmp = MakeFallback();

    auto* ptr = bmp.get();
    cache_[countryCode] = std::move(bmp);
    return ptr;
}
