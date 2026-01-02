#include "StdAfx.h"
#include "ErksMainDialog.h"
#include "ErksUiController.h"
#include "VersionStamp.h"
#include "ErksRuntimeTrace.h"
#include "ErksPopupMenuWnd.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <sstream>
#include <iomanip>

static std::vector<ErksPopupMenuItem> BuildPopupModel(CMenu* menu, CErksMainDialog* dlg);

#include <dwmapi.h>
#include <wincodec.h>
#pragma comment(lib, "dwmapi.lib")

extern HINSTANCE _hdllInstance;

static void ErksPrint(const wchar_t* fmt, ...)
{
    wchar_t buf[1024];
    va_list args;
    va_start(args, fmt);
    _vsnwprintf_s(buf, _countof(buf), _TRUNCATE, fmt, args);
    va_end(args);

    acutPrintf(L"\n[ERKS UI] %s", buf);
}

BEGIN_MESSAGE_MAP(CErksMainDialog, CAdUiDialog)
    ON_WM_CTLCOLOR()
    ON_WM_ERASEBKGND()
    ON_WM_MEASUREITEM()
    ON_WM_DRAWITEM()
    ON_WM_INITMENUPOPUP()
    ON_WM_CLOSE()
    ON_WM_GETMINMAXINFO()
    ON_WM_SIZE()
    ON_WM_NCHITTEST()
    ON_WM_SYSCOMMAND()
    ON_WM_NCLBUTTONDOWN()
    ON_WM_PAINT()
    ON_WM_LBUTTONDOWN()
END_MESSAGE_MAP()

CErksMainDialog::CErksMainDialog(CWnd* pParent)
    : CAdUiDialog(CErksMainDialog::IDD, pParent)
{
}

static void EnableImmersiveDarkTitleBar(HWND hwnd)
{
    if (!hwnd)
        return;

    // DWMWA_USE_IMMERSIVE_DARK_MODE = 20 (1903+) or 19 (1809)
    const BOOL useDark = TRUE;
    ::DwmSetWindowAttribute(hwnd, 20, &useDark, sizeof(useDark));
    ::DwmSetWindowAttribute(hwnd, 19, &useDark, sizeof(useDark));
}

static std::wstring StripAmpersand(const std::wstring& s)
{
    std::wstring out;
    out.reserve(s.size());
    for (wchar_t c : s)
        if (c != L'&')
            out.push_back(c);
    return out;
}

void CErksMainDialog::CaptureMenuText(CMenu* menu)
{
    if (!menu) return;

    const int count = menu->GetMenuItemCount();
    for (int i = 0; i < count; ++i)
    {
        const UINT id = menu->GetMenuItemID(i);
        if (id == (UINT)-1)
        {
            if (CMenu* sub = menu->GetSubMenu(i))
                CaptureMenuText(sub);
            continue;
        }

        CString text;
        menu->GetMenuString(i, text, MF_BYPOSITION);
        m_menuTextById[id] = StripAmpersand(std::wstring(text));
    }
}

void CErksMainDialog::ApplyOwnerDrawToMenu(CMenu* menu)
{
    if (!menu) return;

    const int count = menu->GetMenuItemCount();
    for (int i = 0; i < count; ++i)
    {
        UINT id = menu->GetMenuItemID(i);
        if (id == (UINT)-1)
        {
            if (CMenu* sub = menu->GetSubMenu(i))
                ApplyOwnerDrawToMenu(sub);
            continue;
        }

        // Include separators in owner-draw so we can render them in the same dark theme.
        // Separators have id == 0.
        MENUITEMINFO mii{};
        mii.cbSize = sizeof(mii);
        mii.fMask = MIIM_FTYPE | MIIM_ID;
        if (GetMenuItemInfo(menu->GetSafeHmenu(), i, TRUE, &mii))
        {
            mii.fType |= MFT_OWNERDRAW;
            SetMenuItemInfo(menu->GetSafeHmenu(), i, TRUE, &mii);
        }
    }
}

static HICON CreateIconFromPngResource(HINSTANCE hInst, UINT pngResId, int sizePx)
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

    IWICImagingFactory* factory = nullptr;
    IWICStream* stream = nullptr;
    IWICBitmapDecoder* decoder = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICFormatConverter* converter = nullptr;
    IWICBitmapScaler* scaler = nullptr;

    auto releaseAll = [&]() {
        if (scaler) scaler->Release();
        if (converter) converter->Release();
        if (frame) frame->Release();
        if (decoder) decoder->Release();
        if (stream) stream->Release();
        if (factory) factory->Release();
    };

    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))))
    {
        releaseAll();
        return nullptr;
    }

    if (FAILED(factory->CreateStream(&stream)))
    {
        releaseAll();
        return nullptr;
    }

    if (FAILED(stream->InitializeFromMemory((BYTE*)pData, size)))
    {
        releaseAll();
        return nullptr;
    }

    if (FAILED(factory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnLoad, &decoder))
        )
    {
        releaseAll();
        return nullptr;
    }

    if (FAILED(decoder->GetFrame(0, &frame)))
    {
        releaseAll();
        return nullptr;
    }

    if (FAILED(factory->CreateFormatConverter(&converter)))
    {
        releaseAll();
        return nullptr;
    }

    if (FAILED(converter->Initialize(frame, GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom)))
    {
        releaseAll();
        return nullptr;
    }

    const UINT w = (UINT)sizePx;
    const UINT h = (UINT)sizePx;

    if (FAILED(factory->CreateBitmapScaler(&scaler)))
    {
        releaseAll();
        return nullptr;
    }

    if (FAILED(scaler->Initialize(converter, w, h, WICBitmapInterpolationModeFant)))
    {
        releaseAll();
        return nullptr;
    }

    BITMAPV5HEADER bi;
    ZeroMemory(&bi, sizeof(bi));
    bi.bV5Size = sizeof(bi);
    bi.bV5Width = (LONG)w;
    bi.bV5Height = -(LONG)h;
    bi.bV5Planes = 1;
    bi.bV5BitCount = 32;
    bi.bV5Compression = BI_BITFIELDS;
    bi.bV5RedMask = 0x00FF0000;
    bi.bV5GreenMask = 0x0000FF00;
    bi.bV5BlueMask = 0x000000FF;
    bi.bV5AlphaMask = 0xFF000000;

    void* bits = nullptr;
    HDC hdc = GetDC(nullptr);
    HBITMAP colorBmp = CreateDIBSection(hdc, (BITMAPINFO*)&bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    ReleaseDC(nullptr, hdc);

    if (!colorBmp || !bits)
    {
        if (colorBmp) DeleteObject(colorBmp);
        releaseAll();
        return nullptr;
    }

    const UINT stride = w * 4;
    const UINT bufSize = stride * h;
    if (FAILED(scaler->CopyPixels(nullptr, stride, bufSize, (BYTE*)bits)))
    {
        DeleteObject(colorBmp);
        releaseAll();
        return nullptr;
    }

    HBITMAP maskBmp = CreateBitmap(w, h, 1, 1, nullptr);
    if (!maskBmp)
    {
        DeleteObject(colorBmp);
        releaseAll();
        return nullptr;
    }

    ICONINFO ii;
    ZeroMemory(&ii, sizeof(ii));
    ii.fIcon = TRUE;
    ii.hbmColor = colorBmp;
    ii.hbmMask = maskBmp;

    HICON hIcon = CreateIconIndirect(&ii);

    DeleteObject(maskBmp);
    DeleteObject(colorBmp);

    releaseAll();
    return hIcon;
}

// Helper to get the correct module resource handle (ARX)
static HINSTANCE ErksResourceHandle()
{
    return _hdllInstance ? _hdllInstance : AfxGetResourceHandle();
}

void CErksMainDialog::ForceTopLevelWindow()
{
    if (!GetSafeHwnd())
        return;

    // If the window is owned/parented by AutoCAD, caption buttons and sizing can be suppressed.
    // Clearing GWLP_HWNDPARENT removes both parent/owner for top-level popup windows.
    ::SetWindowLongPtr(GetSafeHwnd(), GWLP_HWNDPARENT, 0);
}

void CErksMainDialog::RefreshFrameStyles()
{
    if (!GetSafeHwnd())
        return;

    LONG_PTR style = ::GetWindowLongPtr(GetSafeHwnd(), GWL_STYLE);
    style &= ~WS_CHILD;
    style |= (WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_THICKFRAME | WS_SIZEBOX);
    style &= ~(DS_MODALFRAME);
    ::SetWindowLongPtr(GetSafeHwnd(), GWL_STYLE, style);

    LONG_PTR exStyle = ::GetWindowLongPtr(GetSafeHwnd(), GWL_EXSTYLE);
    exStyle |= WS_EX_APPWINDOW;
    exStyle &= ~WS_EX_TOOLWINDOW;
    ::SetWindowLongPtr(GetSafeHwnd(), GWL_EXSTYLE, exStyle);

    ::SetWindowPos(GetSafeHwnd(), nullptr, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_NOACTIVATE);
}

BOOL CErksMainDialog::OnInitDialog()
{
    CAdUiDialog::OnInitDialog();

    ErksRuntimeTrace::DumpWindowStyles(GetSafeHwnd(), L"Dialog Before ForceTopLevelWindow");

    // Make it a true top-level window first, then apply frame styles.
    ForceTopLevelWindow();
    RefreshFrameStyles();

    ErksRuntimeTrace::DumpWindowStyles(GetSafeHwnd(), L"Dialog After ForceTopLevelWindow");

    ErksPrint(L"%s", ERKS_BUILD_STAMP_W);
    ErksPrint(L"Resource FileVersion=%s", ERKS_FILE_VERSION_W);

    ErksRuntimeTrace::DumpModuleIdentity(_hdllInstance, L"Dialog OnInit (_hdllInstance)");
    ErksRuntimeTrace::DumpModuleIdentity((HMODULE)AfxGetResourceHandle(), L"Dialog OnInit (AfxGetResourceHandle)");
    ErksRuntimeTrace::DumpWindowStyles(GetSafeHwnd(), L"Dialog OnInit");

    ErksPrint(L"OnInitDialog hwnd=%p", GetSafeHwnd());

    // Force-remove any default buttons if they exist
    if (CWnd* ok = GetDlgItem(IDOK))
    {
        ErksPrint(L"Found IDOK hwnd=%p -> destroying", ok->GetSafeHwnd());
        ok->DestroyWindow();
    }
    if (CWnd* cancel = GetDlgItem(IDCANCEL))
    {
        ErksPrint(L"Found IDCANCEL hwnd=%p -> destroying", cancel->GetSafeHwnd());
        cancel->DestroyWindow();
    }

    RefreshFrameStyles();

    EnableImmersiveDarkTitleBar(GetSafeHwnd());

    // Do not attach a standard window menu; we draw our own menu strip.
    // (Menu resource is still used for popup menus.)

    // Set title bar icon from embedded PNG
    HICON hSmall = CreateIconFromPngResource(ErksResourceHandle(), IDB_ERKS_LOGO_64, 16);
    HICON hBig = CreateIconFromPngResource(ErksResourceHandle(), IDB_ERKS_LOGO_64, 32);
    ErksPrint(L"CreateIcon small=%p big=%p", (void*)hSmall, (void*)hBig);
    if (hSmall)
    {
        SendMessage(WM_SETICON, ICON_SMALL, (LPARAM)hSmall);
        ::SetClassLongPtr(GetSafeHwnd(), GCLP_HICONSM, (LONG_PTR)hSmall);
    }
    if (hBig)
    {
        SendMessage(WM_SETICON, ICON_BIG, (LPARAM)hBig);
        ::SetClassLongPtr(GetSafeHwnd(), GCLP_HICON, (LONG_PTR)hBig);
    }

    if (m_darkBrush.GetSafeHandle() == nullptr)
        m_darkBrush.CreateSolidBrush(m_backColor);

    // Create legacy preview panel (visible by default)
    if (!m_mapPreview.GetSafeHwnd())
    {
        m_mapPreview.Create(this);
        m_mapPreview.SetTheme(m_backColor, m_borderColor, m_textColor);
        m_mapPreview.ShowWindow(SW_SHOW);
    }

    if (CMenu* menu = GetMenu())
    {
        CaptureMenuText(menu);
        ApplyOwnerDrawToMenu(menu);
        DrawMenuBar();
    }

    CString title;
    title.LoadString(IDS_PROJNAME);
    if (!title.IsEmpty())
        SetWindowText(title);

    // AutoCAD-supported sizing: initial size + elastic extents (enables resizing)
    SetDialogMinExtents(960, 540);
    SetDialogMaxExtents(4096, 4096);

    SetWindowPos(nullptr, 0, 0, 1280, 720, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    CenterWindow();

    // Host apps can tweak styles after init; re-apply once.
    RefreshFrameStyles();
    ErksRuntimeTrace::DumpWindowStyles(GetSafeHwnd(), L"Dialog After RefreshFrameStyles");

    return TRUE;
}

LRESULT CErksMainDialog::OnNcHitTest(CPoint point)
{
    // Preserve default non-client hit testing so resize borders and caption buttons work.
    return CAdUiDialog::OnNcHitTest(point);
}

void CErksMainDialog::OnSysCommand(UINT nID, LPARAM lParam)
{
    const UINT cmd = (nID & 0xFFF0);

    switch (cmd)
    {
    case SC_MINIMIZE:
        ShowWindow(SW_MINIMIZE);
        return;
    case SC_MAXIMIZE:
        ShowWindow(SW_MAXIMIZE);
        return;
    case SC_RESTORE:
        ShowWindow(SW_RESTORE);
        return;
    default:
        break;
    }

    CAdUiDialog::OnSysCommand(nID, lParam);
}

HBRUSH CErksMainDialog::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
    pDC->SetTextColor(m_textColor);
    pDC->SetBkColor(m_backColor);

    switch (nCtlColor)
    {
    case CTLCOLOR_DLG:
    case CTLCOLOR_STATIC:
        return (HBRUSH)m_darkBrush.GetSafeHandle();
    case CTLCOLOR_BTN:
    case CTLCOLOR_EDIT:
    case CTLCOLOR_LISTBOX:
        return (HBRUSH)m_darkBrush.GetSafeHandle();
    default:
        return CDialog::OnCtlColor(pDC, pWnd, nCtlColor);
    }
}

BOOL CErksMainDialog::OnCommand(WPARAM wParam, LPARAM lParam)
{
    const UINT id = LOWORD(wParam);

    switch (id)
    {
    case ID_ERKS_FILE_OPEN: OnFileOpen(); return TRUE;
    case ID_ERKS_FILE_SAVE: OnFileSave(); return TRUE;
    case ID_ERKS_FILE_SAVEAS: OnFileSaveAs(); return TRUE;
    case ID_ERKS_FILE_OPEN_EXISTING_DATA: OnFileOpenExistingData(); return TRUE;
    case ID_ERKS_FILE_CREATE_ROAD_AXIS: OnFileCreateRoadAxis(); return TRUE;
    case ID_ERKS_FILE_EXIT: OnFileExit(); return TRUE;
    case ID_ERKS_HELP_ABOUT: OnHelpAbout(); return TRUE;
    default:
        return CDialog::OnCommand(wParam, lParam);
    }
}

void CErksMainDialog::OnFileOpen() { }
void CErksMainDialog::OnFileSave() { }
void CErksMainDialog::OnFileSaveAs() { }
void CErksMainDialog::OnFileCreateRoadAxis() { }

void CErksMainDialog::OnFileExit()
{
    DestroyWindow();
}

void CErksMainDialog::OnHelpAbout()
{
    AfxMessageBox(_T("Erk-S V1.0.0.0.1\n\nAbout"), MB_OK | MB_ICONINFORMATION);
}

void CErksMainDialog::OnInitMenuPopup(CMenu* pPopupMenu, UINT nIndex, BOOL bSysMenu)
{
    CDialog::OnInitMenuPopup(pPopupMenu, nIndex, bSysMenu);

    if (pPopupMenu && !bSysMenu)
    {
        CaptureMenuText(pPopupMenu);
        ApplyOwnerDrawToMenu(pPopupMenu);
    }
}

void CErksMainDialog::OnMeasureItem(int nIDCtl, LPMEASUREITEMSTRUCT lpMeasureItemStruct)
{
    CDialog::OnMeasureItem(nIDCtl, lpMeasureItemStruct);

    if (!lpMeasureItemStruct || lpMeasureItemStruct->CtlType != ODT_MENU)
        return;

    // Separator items have itemID == 0 when using standard menus.
    if (lpMeasureItemStruct->itemID == 0)
    {
        lpMeasureItemStruct->itemHeight = 10; // padding around the line
        lpMeasureItemStruct->itemWidth = 1;   // width ignored by the system for menus
        return;
    }

    auto it = m_menuTextById.find((UINT)lpMeasureItemStruct->itemID);
    const std::wstring text = (it != m_menuTextById.end()) ? it->second : L"";

    CClientDC dc(this);
    CFont* oldFont = dc.SelectObject(GetFont());
    CSize sz = dc.GetTextExtent(text.c_str());
    dc.SelectObject(oldFont);

    // Match the popup menu's item height calculation: max(18, fontH + 4)
    const UINT desiredHeight = (UINT)sz.cy + 4;
    lpMeasureItemStruct->itemHeight = (desiredHeight > 18u) ? desiredHeight : 18u;
    
    // Match the popup menu's width calculation: textWidth + 50 (for padding and margins)
    lpMeasureItemStruct->itemWidth = (UINT)sz.cx + 50;
}

void CErksMainDialog::OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct)
{
    CDialog::OnDrawItem(nIDCtl, lpDrawItemStruct);

    if (!lpDrawItemStruct || lpDrawItemStruct->CtlType != ODT_MENU)
        return;

    CDC dc;
    dc.Attach(lpDrawItemStruct->hDC);

    CRect rc(lpDrawItemStruct->rcItem);

    // Draw separator as a thin gray line on the dark background.
    if (lpDrawItemStruct->itemID == 0)
    {
        dc.FillSolidRect(&rc, m_backColor);

        const int y = rc.top + (rc.Height() / 2);
        const int left = rc.left + 12;
        const int right = rc.right - 12;

        CPen pen(PS_SOLID, 1, m_borderColor);
        CPen* oldPen = dc.SelectObject(&pen);
        dc.MoveTo(left, y);
        dc.LineTo(right, y);
        dc.SelectObject(oldPen);

        dc.Detach();
        return;
    }

    const UINT itemState = lpDrawItemStruct->itemState;
    const bool selected = (itemState & ODS_SELECTED) != 0;
    const bool disabled = (itemState & (ODS_DISABLED | ODS_GRAYED)) != 0;

    // Fill entire item rect to eliminate light menu bar background bleed.
    const COLORREF back = selected ? m_hotBackColor : m_backColor;
    const COLORREF text = disabled ? RGB(140, 140, 140) : m_textColor;
    dc.FillSolidRect(&rc, back);

    if (selected)
    {
        // Use a thin gray outline instead of the default thick/bright selection border.
        CRect frame = rc;
        frame.DeflateRect(1, 1);

        CPen pen(PS_SOLID, 1, m_borderColor);
        CPen* oldPen = dc.SelectObject(&pen);
        HBRUSH oldBrush = (HBRUSH)dc.SelectStockObject(NULL_BRUSH);
        dc.Rectangle(&frame);
        dc.SelectObject(oldBrush);
        dc.SelectObject(oldPen);
    }

    auto it = m_menuTextById.find((UINT)lpDrawItemStruct->itemID);
    std::wstring caption = (it != m_menuTextById.end()) ? it->second : L"";

    dc.SetBkMode(TRANSPARENT);
    dc.SetTextColor(text);

    CFont* oldFont = dc.SelectObject(GetFont());

    // Use the same padding as the popup menu (paddingX = 12)
    rc.DeflateRect(12, 0);
    dc.DrawText(caption.c_str(), (int)caption.size(), &rc, DT_SINGLELINE | DT_VCENTER | DT_LEFT);

    dc.SelectObject(oldFont);
    dc.Detach();
}

void CErksMainDialog::RecalcMenuStripRects()
{
    CRect rc;
    GetClientRect(&rc);

    const int paddingX = 10;
    const int gap = 6;

    CClientDC dc(this);
    CFont* oldFont = dc.SelectObject(GetFont());

    const CString fileText = _T("File");
    const CString aboutText = _T("About");

    CSize fileSz = dc.GetTextExtent(fileText);
    CSize aboutSz = dc.GetTextExtent(aboutText);

    dc.SelectObject(oldFont);

    const int y0 = 0;
    const int y1 = m_menuStripHeight;

    int x = paddingX;
    m_rcMenuFile = CRect(x, y0, x + fileSz.cx + 24, y1);
    x = m_rcMenuFile.right + gap;
    m_rcMenuAbout = CRect(x, y0, x + aboutSz.cx + 24, y1);
}

void CErksMainDialog::DrawMenuStrip(CDC& dc)
{
    CRect rc;
    GetClientRect(&rc);

    CRect band = rc;
    band.bottom = band.top + m_menuStripHeight;

    dc.FillSolidRect(&band, m_backColor);

    CFont* oldFont = dc.SelectObject(GetFont());
    dc.SetBkMode(TRANSPARENT);
    dc.SetTextColor(m_textColor);

    auto drawItem = [&](const CRect& r, const TCHAR* text) {
        CRect t = r;
        dc.DrawText(text, -1, &t, DT_SINGLELINE | DT_VCENTER | DT_CENTER);
    };

    drawItem(m_rcMenuFile, _T("File"));
    drawItem(m_rcMenuAbout, _T("About"));

    dc.SelectObject(oldFont);
}

void CErksMainDialog::ShowTopMenuPopup(int topIndex, const CRect& rcItem)
{
    if (!m_mainMenu.GetSafeHmenu())
    {
        if (!m_mainMenu.LoadMenu(IDM_ERKS_MAIN))
            return;
        CaptureMenuText(&m_mainMenu);
        ApplyOwnerDrawToMenu(&m_mainMenu);
    }

    CMenu* top = m_mainMenu.GetSubMenu(topIndex);
    if (!top)
        return;

    // Where to show
    CPoint pt(rcItem.left, rcItem.bottom);
    ClientToScreen(&pt);

    // Build model from submenu
    const std::vector<ErksPopupMenuItem> items = BuildPopupModel(top, this);

    CErksPopupMenuWnd popup;
    UINT cmd = popup.Track(this, pt, items, m_backColor, m_textColor, m_hotBackColor, m_borderColor);
    if (cmd != 0)
        PostMessage(WM_COMMAND, cmd, 0);
}

void CErksMainDialog::OnPaint()
{
    CPaintDC dc(this);

    RecalcMenuStripRects();
    DrawMenuStrip(dc);
}

void CErksMainDialog::OnLButtonDown(UINT nFlags, CPoint point)
{
    RecalcMenuStripRects();

    if (m_rcMenuFile.PtInRect(point))
    {
        ShowTopMenuPopup(0, m_rcMenuFile);
        return;
    }

    if (m_rcMenuAbout.PtInRect(point))
    {
        ShowTopMenuPopup(1, m_rcMenuAbout);
        return;
    }

    CAdUiDialog::OnLButtonDown(nFlags, point);
}

void CErksMainDialog::OnNcLButtonDown(UINT nHitTest, CPoint point)
{
    // Let the default handler manage non-client interactions (sizing, system buttons).
    CAdUiDialog::OnNcLButtonDown(nHitTest, point);
}

BOOL CErksMainDialog::OnEraseBkgnd(CDC* pDC)
{
    // Paint full client background dark; OnPaint draws the menu strip.
    if (pDC)
    {
        CRect rc;
        GetClientRect(&rc);
        pDC->FillSolidRect(&rc, m_backColor);
    }
    return TRUE;
}

void CErksMainDialog::PostNcDestroy()
{
    CDialog::PostNcDestroy();
    CErksUiController::Instance().ClearDialog();
    delete this;
}

void CErksMainDialog::OnOK()
{
    // No default OK behavior
}

void CErksMainDialog::OnCancel()
{
    // No default Cancel behavior
}

void CErksMainDialog::OnClose()
{
    DestroyWindow();
}

void CErksMainDialog::OnGetMinMaxInfo(MINMAXINFO* lpMMI)
{
    CAdUiDialog::OnGetMinMaxInfo(lpMMI);
    if (!lpMMI)
        return;

    lpMMI->ptMinTrackSize.x = 960;
    lpMMI->ptMinTrackSize.y = 540;

    lpMMI->ptMaxTrackSize.x = 4096;
    lpMMI->ptMaxTrackSize.y = 4096;
}

void CErksMainDialog::OnSize(UINT nType, int cx, int cy)
{
    CAdUiDialog::OnSize(nType, cx, cy);

    const int margin = 12;
    const int top = m_menuStripHeight + margin;
    const int bottom = margin;

    int w = cx - (margin * 2);
    int h = cy - top - bottom;
    if (w < 0) w = 0;
    if (h < 0) h = 0;

    if (m_mapPreview.GetSafeHwnd())
    {
        m_mapPreview.ShowWindow(SW_SHOW);
        m_mapPreview.SetWindowPos(nullptr, margin, top, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
    }

    Invalidate(FALSE);
}

static std::vector<ErksPopupMenuItem> BuildPopupModel(CMenu* menu, CErksMainDialog* dlg)
{
    std::vector<ErksPopupMenuItem> items;
    if (!menu)
        return items;

    const int count = menu->GetMenuItemCount();
    items.reserve((size_t)count);

    for (int i = 0; i < count; ++i)
    {
        UINT id = menu->GetMenuItemID(i);

        // separator
        if (id == 0)
        {
            items.push_back(ErksPopupMenuItem{});
            continue;
        }

        // submenus are not supported in this custom popup yet; skip.
        if (id == (UINT)-1)
            continue;

        MENUITEMINFO mii{};
        mii.cbSize = sizeof(mii);
        mii.fMask = MIIM_STATE;
        bool enabled = true;
        if (GetMenuItemInfo(menu->GetSafeHmenu(), i, TRUE, &mii))
            enabled = (mii.fState & (MFS_DISABLED | MFS_GRAYED)) == 0;

        std::wstring text;
        if (dlg)
        {
            if (const std::wstring* cached = dlg->TryGetMenuText(id))
                text = *cached;
        }
        if (text.empty())
        {
            CString t;
            menu->GetMenuString(i, t, MF_BYPOSITION);
            text = StripAmpersand(std::wstring(t));
        }

        ErksPopupMenuItem item;
        item.id = id;
        item.text = text;
        item.enabled = enabled;
        items.push_back(std::move(item));
    }

    return items;
}

const std::wstring* CErksMainDialog::TryGetMenuText(UINT id) const
{
    auto it = m_menuTextById.find(id);
    if (it == m_menuTextById.end())
        return nullptr;
    return &it->second;
}

static bool ErksLayerEquals(const ACHAR* a, const wchar_t* b)
{
    if (!a || !b) return false;
    CStringW wa(a);
    wa.MakeLower();
    CStringW wb(b);
    wb.MakeLower();
    return wa == wb;
}

static void AddSegment(CErksMapPreviewWnd::Polyline2d& pl, const AcGePoint3d& p)
{
    pl.pts.push_back(CErksMapPreviewWnd::PointF((float)p.x, (float)p.y));
}

static void AddArcApprox(CErksMapPreviewWnd::Polyline2d& pl, const AcGePoint3d& center, double radius, double startAng, double endAng, int steps)
{
    if (steps < 4) steps = 4;

    double da = endAng - startAng;
    while (da <= 0.0) da += (2.0 * 3.14159265358979323846);

    for (int i = 0; i <= steps; ++i)
    {
        const double t = (double)i / (double)steps;
        const double a = startAng + (da * t);
        const double x = center.x + radius * cos(a);
        const double y = center.y + radius * sin(a);
        pl.pts.push_back(CErksMapPreviewWnd::PointF((float)x, (float)y));
    }
}

static void AddCircleApprox(CErksMapPreviewWnd::Polyline2d& pl, const AcGePoint3d& center, double radius, int steps)
{
    if (steps < 8) steps = 8;
    for (int i = 0; i < steps; ++i)
    {
        const double a = (2.0 * 3.14159265358979323846) * ((double)i / (double)steps);
        const double x = center.x + radius * cos(a);
        const double y = center.y + radius * sin(a);
        pl.pts.push_back(CErksMapPreviewWnd::PointF((float)x, (float)y));
    }
    pl.closed = true;
}

static bool ExtractEntityAsPolyline(const AcDbEntity* ent, CErksMapPreviewWnd::Polyline2d& out)
{
    if (!ent)
        return false;

    if (const AcDbLine* ln = AcDbLine::cast(ent))
    {
        AddSegment(out, ln->startPoint());
        AddSegment(out, ln->endPoint());
        return true;
    }

    if (const AcDbArc* arc = AcDbArc::cast(ent))
    {
        const double r = arc->radius();
        const double sweep = arc->endAngle() - arc->startAngle();
        const int steps = std::max(12, (int)std::ceil(std::abs(sweep) / (3.14159265358979323846 / 18.0))); // ~10 deg
        AddArcApprox(out, arc->center(), r, arc->startAngle(), arc->endAngle(), steps);
        return true;
    }

    if (const AcDbCircle* cir = AcDbCircle::cast(ent))
    {
        AddCircleApprox(out, cir->center(), cir->radius(), 72);
        return true;
    }

    if (const AcDbPolyline* pl = AcDbPolyline::cast(ent))
    {
        const int n = pl->numVerts();
        if (n <= 0)
            return false;

        for (int i = 0; i < n; ++i)
        {
            AcGePoint3d p;
            pl->getPointAt(i, p);
            out.pts.push_back(CErksMapPreviewWnd::PointF((float)p.x, (float)p.y));
        }

        out.closed = pl->isClosed();
        return true;
    }

    if (const AcDb2dPolyline* pl2 = AcDb2dPolyline::cast(ent))
    {
        AcDbObjectIterator* it = pl2->vertexIterator();
        if (!it) return false;

        std::unique_ptr<AcDbObjectIterator> itGuard(it);

        for (; !it->done(); it->step())
        {
            AcDbObjectId vid = it->objectId();
            AcDbObject* obj = nullptr;
            if (acdbOpenObject(obj, vid, AcDb::kForRead) != Acad::eOk || !obj)
                continue;

            AcDb2dVertex* v = AcDb2dVertex::cast(obj);
            if (v)
            {
                const AcGePoint3d p = v->position();
                out.pts.push_back(CErksMapPreviewWnd::PointF((float)p.x, (float)p.y));
            }
            obj->close();
        }

        out.closed = pl2->isClosed();
        return out.pts.size() >= 2;
    }

    if (const AcDbSpline* sp = AcDbSpline::cast(ent))
    {
        // Sample spline by parameter (portable across ARX versions)
        const int steps = 64;
        double startP = 0.0, endP = 1.0;
        if (sp->getStartParam(startP) == Acad::eOk && sp->getEndParam(endP) == Acad::eOk)
        {
            for (int i = 0; i <= steps; ++i)
            {
                const double t = startP + (endP - startP) * ((double)i / (double)steps);
                AcGePoint3d p;
                if (sp->getPointAtParam(t, p) == Acad::eOk)
                    out.pts.push_back(CErksMapPreviewWnd::PointF((float)p.x, (float)p.y));
            }
            return out.pts.size() >= 2;
        }

        return false;
    }

    return false;
}

void CErksMainDialog::OnFileOpenExistingData()
{
    CFileDialog dlg(TRUE, _T("dwg"), nullptr,
        OFN_FILEMUSTEXIST | OFN_HIDEREADONLY,
        _T("AutoCAD Drawing (*.dwg)|*.dwg|All Files (*.*)|*.*||"),
        this);

    if (dlg.DoModal() != IDOK)
        return;

    const CString path = dlg.GetPathName();
    if (path.IsEmpty())
        return;

    std::unique_ptr<AcDbDatabase> db(new AcDbDatabase(false, true));

    const Acad::ErrorStatus esRead = db->readDwgFile((LPCTSTR)path);
    if (esRead != Acad::eOk)
    {
        CString msg;
        msg.Format(_T("Failed to read DWG: %s (Error %d)"), path.GetString(), (int)esRead);
        AfxMessageBox(msg, MB_OK | MB_ICONERROR);
        return;
    }

    // Compute extents
    db->updateExt();
    const AcGePoint3d mn = db->extmin();
    const AcGePoint3d mx = db->extmax();

    ErksPrint(L"DWG extents min=(%.3f,%.3f) max=(%.3f,%.3f)", mn.x, mn.y, mx.x, mx.y);

    // --- Extract entities for legacy preview

    AcDbBlockTable* bt = nullptr;
    if (db->getBlockTable(bt, AcDb::kForRead) != Acad::eOk || !bt)
    {
        AfxMessageBox(_T("Failed to open block table."), MB_OK | MB_ICONERROR);
        return;
    }

    AcDbBlockTableRecord* ms = nullptr;
    if (bt->getAt(ACDB_MODEL_SPACE, ms, AcDb::kForRead) != Acad::eOk || !ms)
    {
        bt->close();
        AfxMessageBox(_T("Failed to open model space."), MB_OK | MB_ICONERROR);
        return;
    }

    bt->close();

    std::vector<CErksMapPreviewWnd::Polyline2d> polys;

    bool foundMinor = false;
    bool foundMajor = false;

    AcDbBlockTableRecordIterator* it = nullptr;
    if (ms->newIterator(it) != Acad::eOk || !it)
    {
        ms->close();
        AfxMessageBox(_T("Failed to iterate model space."), MB_OK | MB_ICONERROR);
        return;
    }

    std::unique_ptr<AcDbBlockTableRecordIterator> itGuard(it);

    for (; !it->done(); it->step())
    {
        AcDbEntity* ent = nullptr;
        if (it->getEntity(ent, AcDb::kForRead) != Acad::eOk || !ent)
            continue;

        const ACHAR* layer = ent->layer();
        const bool isMinor = ErksLayerEquals(layer, L"minor");
        const bool isMajor = ErksLayerEquals(layer, L"major");

        if (!isMinor && !isMajor)
        {
            ent->close();
            continue;
        }

        if (isMinor) foundMinor = true;
        if (isMajor) foundMajor = true;

        CErksMapPreviewWnd::Polyline2d pl;
        pl.color = isMajor ? RGB(230, 230, 230) : RGB(160, 160, 160);

        const bool ok = ExtractEntityAsPolyline(ent, pl);
        ent->close();

        if (!(ok && pl.pts.size() >= 2))
            continue;

        polys.push_back(pl);
    }

    ms->close();

    if (m_mapPreview.GetSafeHwnd())
        m_mapPreview.SetGeometry(polys);

    if (!foundMinor && !foundMajor)
        AfxMessageBox(_T("No entities found on layers 'minor' or 'major'."), MB_OK | MB_ICONWARNING);
}
