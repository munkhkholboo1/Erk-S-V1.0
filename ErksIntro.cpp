#include "StdAfx.h"
#include "ErksIntro.h"

#include <wincodec.h>
#pragma comment(lib, "windowscodecs.lib")

#include "Resource.h"

CErksIntro::CErksIntro() {}

CErksIntro::~CErksIntro()
{
    CloseNow();
}

BEGIN_MESSAGE_MAP(CErksIntro, CWnd)
    ON_WM_CREATE()
    ON_WM_DESTROY()
    ON_WM_TIMER()
    ON_WM_PAINT()
END_MESSAGE_MAP()

static IWICImagingFactory* GetWicFactory()
{
    static IWICImagingFactory* s_factory = nullptr;
    if (s_factory)
        return s_factory;

    IWICImagingFactory* f = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&f))))
        s_factory = f;
    return s_factory;
}

static HBITMAP CreateHBitmapFromPngResource(HINSTANCE hInst, UINT pngResId, int targetW, int targetH)
{
    HRSRC hRes = FindResource(hInst, MAKEINTRESOURCE(pngResId), L"PNG");
    if (!hRes)
        return nullptr;

    HGLOBAL hData = LoadResource(hInst, hRes);
    if (!hData)
        return nullptr;

    DWORD size = SizeofResource(hInst, hRes);
    const void* pData = LockResource(hData);
    if (!pData || size == 0)
        return nullptr;

    IWICImagingFactory* factory = GetWicFactory();
    if (!factory)
        return nullptr;

    IWICStream* stream = nullptr;
    IWICBitmapDecoder* decoder = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICFormatConverter* converter = nullptr;
    IWICBitmapScaler* scaler = nullptr;

    auto cleanup = [&]() {
        if (scaler) scaler->Release();
        if (converter) converter->Release();
        if (frame) frame->Release();
        if (decoder) decoder->Release();
        if (stream) stream->Release();
    };

    if (FAILED(factory->CreateStream(&stream)))
    {
        cleanup();
        return nullptr;
    }

    if (FAILED(stream->InitializeFromMemory((BYTE*)pData, size)))
    {
        cleanup();
        return nullptr;
    }

    if (FAILED(factory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnLoad, &decoder)))
    {
        cleanup();
        return nullptr;
    }

    if (FAILED(decoder->GetFrame(0, &frame)))
    {
        cleanup();
        return nullptr;
    }

    if (FAILED(factory->CreateFormatConverter(&converter)))
    {
        cleanup();
        return nullptr;
    }

    if (FAILED(converter->Initialize(frame, GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom)))
    {
        cleanup();
        return nullptr;
    }

    if (FAILED(factory->CreateBitmapScaler(&scaler)))
    {
        cleanup();
        return nullptr;
    }

    if (FAILED(scaler->Initialize(converter, (UINT)targetW, (UINT)targetH, WICBitmapInterpolationModeFant)))
    {
        cleanup();
        return nullptr;
    }

    BITMAPV5HEADER bi;
    ZeroMemory(&bi, sizeof(bi));
    bi.bV5Size = sizeof(bi);
    bi.bV5Width = targetW;
    bi.bV5Height = -targetH;
    bi.bV5Planes = 1;
    bi.bV5BitCount = 32;
    bi.bV5Compression = BI_BITFIELDS;
    bi.bV5RedMask = 0x00FF0000;
    bi.bV5GreenMask = 0x0000FF00;
    bi.bV5BlueMask = 0x000000FF;
    bi.bV5AlphaMask = 0xFF000000;

    void* bits = nullptr;
    HDC hdc = GetDC(nullptr);
    HBITMAP hbmp = CreateDIBSection(hdc, (BITMAPINFO*)&bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    ReleaseDC(nullptr, hdc);

    if (!hbmp || !bits)
    {
        if (hbmp) DeleteObject(hbmp);
        cleanup();
        return nullptr;
    }

    const UINT stride = (UINT)targetW * 4;
    const UINT bufSize = stride * (UINT)targetH;
    if (FAILED(scaler->CopyPixels(nullptr, stride, bufSize, (BYTE*)bits)))
    {
        DeleteObject(hbmp);
        cleanup();
        return nullptr;
    }

    cleanup();
    return hbmp;
}

bool CErksIntro::LoadIntroBitmap()
{
    if (m_bitmap)
        return true;

    m_bitmap = CreateHBitmapFromPngResource(AfxGetResourceHandle(), IDB_ERKS_INTRO_PNG, m_width, m_height);
    return m_bitmap != nullptr;
}

bool CErksIntro::Show(int width, int height, DWORD durationMs, HWND notifyHwnd, UINT notifyMsg, WPARAM notifyWParam, LPARAM notifyLParam)
{
    m_width = width;
    m_height = height;
    m_durationMs = durationMs;

    m_notifyHwnd = notifyHwnd;
    m_notifyMsg = notifyMsg;
    m_notifyWParam = notifyWParam;
    m_notifyLParam = notifyLParam;

    // WIC uses COM
    CoInitialize(nullptr);

    CRect rc(0, 0, m_width, m_height);
    DWORD style = WS_POPUP | WS_VISIBLE;
    DWORD exStyle = WS_EX_TOOLWINDOW | WS_EX_TOPMOST;

    if (!CreateEx(exStyle, AfxRegisterWndClass(0), _T(""), style, rc, nullptr, 0))
        return false;

    CenterWindow();
    ::SetWindowPos(GetSafeHwnd(), HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    UpdateWindow();

    m_closeTimer = SetTimer(1, m_durationMs, nullptr);
    return true;
}

int CErksIntro::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    if (CWnd::OnCreate(lpCreateStruct) == -1)
        return -1;

    LoadIntroBitmap();
    return 0;
}

void CErksIntro::OnPaint()
{
    CPaintDC dc(this);
    CRect rc;
    GetClientRect(&rc);

    dc.FillSolidRect(&rc, RGB(0, 0, 0));

    if (!m_bitmap)
        return;

    CDC mem;
    mem.CreateCompatibleDC(&dc);
    HGDIOBJ old = mem.SelectObject(m_bitmap);
    dc.BitBlt(0, 0, rc.Width(), rc.Height(), &mem, 0, 0, SRCCOPY);
    mem.SelectObject(old);
}

void CErksIntro::OnDestroy()
{
    if (m_closeTimer)
    {
        KillTimer(m_closeTimer);
        m_closeTimer = 0;
    }

    if (m_bitmap)
    {
        DeleteObject(m_bitmap);
        m_bitmap = nullptr;
    }

    if (m_notifyHwnd && m_notifyMsg)
        ::PostMessage(m_notifyHwnd, m_notifyMsg, m_notifyWParam, m_notifyLParam);

    CWnd::OnDestroy();
}

void CErksIntro::PostNcDestroy()
{
    CWnd::PostNcDestroy();
    delete this;
}

void CErksIntro::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == 1)
    {
        CloseNow();
        return;
    }

    CWnd::OnTimer(nIDEvent);
}

void CErksIntro::CloseNow()
{
    if (GetSafeHwnd())
        DestroyWindow();
}
