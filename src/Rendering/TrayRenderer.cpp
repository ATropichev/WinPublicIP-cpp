#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include "TrayRenderer.h"

static IStream* NamedResourceToStream(const char* name)
{
    HMODULE hMod = GetModuleHandleW(nullptr);
    HRSRC hRes = FindResourceA(hMod, name, MAKEINTRESOURCEA(10));
    if (!hRes) return nullptr;
    HGLOBAL hGlob = LoadResource(hMod, hRes);
    DWORD size = SizeofResource(hMod, hRes);
    void* data = LockResource(hGlob);
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, size);
    void* buf = GlobalLock(hMem);
    memcpy(buf, data, size);
    GlobalUnlock(hMem);
    IStream* stream = nullptr;
    CreateStreamOnHGlobal(hMem, TRUE, &stream);
    return stream;
}

static std::unique_ptr<Gdiplus::Bitmap> LoadNamedResource(const char* name)
{
    IStream* stream = NamedResourceToStream(name);
    if (!stream) return nullptr;
    auto bmp = std::make_unique<Gdiplus::Bitmap>(stream);
    stream->Release();
    if (bmp->GetLastStatus() != Gdiplus::Ok) return nullptr;
    return bmp;
}

static std::unique_ptr<Gdiplus::Bitmap> MakePlaceholder(Gdiplus::ARGB color)
{
    auto bmp = std::make_unique<Gdiplus::Bitmap>(32, 32, PixelFormat32bppARGB);
    Gdiplus::Graphics g(bmp.get());
    Gdiplus::Color c(color);
    Gdiplus::SolidBrush br(c);
    g.FillRectangle(&br, 0, 0, 32, 32);
    return bmp;
}

TrayRenderer::TrayRenderer()
{
    shieldOn_  = LoadNamedResource("WPI_SHIELD_ON");
    shieldOff_ = LoadNamedResource("WPI_SHIELD_OFF");
    offline_   = LoadNamedResource("WPI_OFFLINE");
    if (!shieldOn_)  shieldOn_  = MakePlaceholder(0xFF22BB22);
    if (!shieldOff_) shieldOff_ = MakePlaceholder(0xFF888888);
    if (!offline_)   offline_   = MakePlaceholder(0xFFCC2222);
}

TrayRenderer::~TrayRenderer()
{
    if (lastIcon_) DestroyIcon(lastIcon_);
}

HICON TrayRenderer::Compose(Gdiplus::Bitmap* bg, Gdiplus::Bitmap* overlay)
{
    Gdiplus::Bitmap canvas(32, 32, PixelFormat32bppARGB);
    {
        Gdiplus::Graphics g(&canvas);
        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        g.DrawImage(bg, 0, 0, 32, 32);
        if (overlay)
            g.DrawImage(overlay, 16, 16, 16, 16);
    }

    HBITMAP hBmp = nullptr;
    canvas.GetHBITMAP(Gdiplus::Color(0, 0, 0, 0), &hBmp);

    // Маска: 1-bpp битмап (для 32-бит иконок Windows использует альфа-канал,
    // маска игнорируется — но ICONINFO требует валидный hbmMask).
    // CreateBitmap не требует DC, в отличие от CreateCompatibleBitmap(GetDC(nullptr),...).
    ICONINFO ii{};
    ii.fIcon    = TRUE;
    ii.hbmColor = hBmp;
    ii.hbmMask  = CreateBitmap(32, 32, 1, 1, nullptr);
    HICON icon  = CreateIconIndirect(&ii);

    DeleteObject(ii.hbmColor);
    DeleteObject(ii.hbmMask);

    if (lastIcon_) DestroyIcon(lastIcon_);
    lastIcon_ = icon;
    return icon;
}

HICON TrayRenderer::Render(const std::string& countryCode, bool vpnOn)
{
    Gdiplus::Bitmap* flag    = flagLoader_.Load(countryCode);
    Gdiplus::Bitmap* shield  = vpnOn ? shieldOn_.get() : shieldOff_.get();
    return Compose(flag, shield);
}

HICON TrayRenderer::RenderOffline()
{
    return Compose(offline_.get(), nullptr);
}
