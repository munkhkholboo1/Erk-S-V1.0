#include "StdAfx.h"
#include "ErksMapPreviewWnd.h"

#include <algorithm>
#include <cmath>

BEGIN_MESSAGE_MAP(CErksMapPreviewWnd, CWnd)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_MOUSEWHEEL()
    ON_WM_MBUTTONDOWN()
    ON_WM_MBUTTONUP()
    ON_WM_MOUSEMOVE()
    ON_WM_KEYDOWN()
    ON_WM_CONTEXTMENU()
    ON_WM_LBUTTONDBLCLK()
    ON_WM_SIZE()
    ON_BN_CLICKED(IDC_PREVIEW_RELOAD, &CErksMapPreviewWnd::OnReloadOverlayClicked)
    ON_WM_DRAWITEM()
END_MESSAGE_MAP()

void CErksMapPreviewWnd::SetReloadTarget(CWnd* target, UINT commandId)
{
    m_reloadTarget = target;
    m_reloadCommandId = commandId;
}

void CErksMapPreviewWnd::SetReloadEnabled(bool enabled)
{
    if (m_btnReload.GetSafeHwnd())
        m_btnReload.EnableWindow(enabled ? TRUE : FALSE);
}

BOOL CErksMapPreviewWnd::Create(CWnd* pParent)
{
    if (!pParent || !::IsWindow(pParent->GetSafeHwnd()))
        return FALSE;

    CString cls = AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW, ::LoadCursor(nullptr, IDC_ARROW), (HBRUSH)GetStockObject(NULL_BRUSH), nullptr);
    const BOOL ok = CWnd::CreateEx(0, cls, _T("ERKS_MAP_PREVIEW"), WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), pParent, 0);
    if (ok)
    {
        // Create overlay reload icon button inside preview (no extra layout space).
        if (!m_btnReload.GetSafeHwnd())
        {
            m_btnReload.Create(_T(""), WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, CRect(0, 0, 0, 0), this, IDC_PREVIEW_RELOAD);
            m_btnReload.EnableWindow(FALSE);
        }
        LayoutOverlay();
    }
    return ok;
}

void CErksMapPreviewWnd::OnSize(UINT nType, int cx, int cy)
{
    CWnd::OnSize(nType, cx, cy);
    UNREFERENCED_PARAMETER(nType);
    UNREFERENCED_PARAMETER(cx);
    UNREFERENCED_PARAMETER(cy);
    LayoutOverlay();
}

void CErksMapPreviewWnd::LayoutOverlay()
{
    if (!GetSafeHwnd() || !m_btnReload.GetSafeHwnd())
        return;

    CRect rc;
    GetClientRect(&rc);

    const int pad = 10;
    const int sz = 26;
    const int x = std::max(0, (int)rc.right - pad - sz);
    const int y = std::max(0, (int)rc.top + pad);

    m_btnReload.SetWindowPos(nullptr, x, y, sz, sz, SWP_NOZORDER | SWP_NOACTIVATE);
}

void CErksMapPreviewWnd::OnReloadOverlayClicked()
{
    if (m_reloadTarget && ::IsWindow(m_reloadTarget->GetSafeHwnd()) && m_reloadCommandId != 0)
        m_reloadTarget->PostMessage(WM_COMMAND, (WPARAM)m_reloadCommandId, 0);
}

void CErksMapPreviewWnd::OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct)
{
    CWnd::OnDrawItem(nIDCtl, lpDrawItemStruct);
    if (!lpDrawItemStruct)
        return;

    if (lpDrawItemStruct->CtlType != ODT_BUTTON || lpDrawItemStruct->CtlID != IDC_PREVIEW_RELOAD)
        return;

    CDC dc;
    dc.Attach(lpDrawItemStruct->hDC);

    CRect rc(lpDrawItemStruct->rcItem);

    const UINT state = lpDrawItemStruct->itemState;
    const bool disabled = (state & ODS_DISABLED) != 0;
    const bool pressed = (state & ODS_SELECTED) != 0;

    const COLORREF back = disabled ? RGB(40, 40, 40) : (pressed ? RGB(35, 35, 38) : RGB(45, 45, 48));
    const COLORREF border = RGB(80, 80, 80);
    const COLORREF icon = disabled ? RGB(120, 120, 120) : RGB(230, 230, 230);

    dc.FillSolidRect(&rc, back);

    CPen pen(PS_SOLID, 1, border);
    CPen* oldPen = dc.SelectObject(&pen);
    HBRUSH oldBrush = (HBRUSH)dc.SelectStockObject(NULL_BRUSH);
    dc.RoundRect(&rc, CPoint(6, 6));
    dc.SelectObject(oldBrush);
    dc.SelectObject(oldPen);

    CRect glyph = rc;
    glyph.DeflateRect(6, 6);
    DrawReloadGlyph(dc, glyph, icon);

    dc.Detach();
}

void CErksMapPreviewWnd::DrawReloadGlyph(CDC& dc, const CRect& rc, COLORREF color)
{
    // Simple circular-arrow glyph using GDI primitives.
    const int w = rc.Width();
    const int h = rc.Height();
    const int r = std::min(w, h) / 2;
    const int cx = rc.left + w / 2;
    const int cy = rc.top + h / 2;

    const int thickness = 2;
    CPen pen(PS_SOLID, thickness, color);
    CPen* oldPen = dc.SelectObject(&pen);

    // Arc
    CRect arcRc(cx - r, cy - r, cx + r, cy + r);
    dc.Arc(&arcRc, CPoint(cx, cy - r), CPoint(cx + r, cy));

    // Arrow head (right side)
    const int ax = cx + r - 1;
    const int ay = cy;
    dc.MoveTo(ax, ay);
    dc.LineTo(ax - 6, ay - 4);
    dc.MoveTo(ax, ay);
    dc.LineTo(ax - 6, ay + 4);

    dc.SelectObject(oldPen);
}

void CErksMapPreviewWnd::SetTheme(COLORREF backColor, COLORREF borderColor, COLORREF textColor)
{
    m_backColor = backColor;
    m_borderColor = borderColor;
    m_textColor = textColor;

    if (GetSafeHwnd())
        Invalidate(FALSE);
}

void CErksMapPreviewWnd::SetGeometry(const std::vector<Polyline2d>& polys)
{
    m_polys = polys;
    ComputeWorldBounds();
    m_viewInit = false;
    if (GetSafeHwnd())
        Invalidate(FALSE);
}

void CErksMapPreviewWnd::ComputeWorldBounds()
{
    m_hasBounds = false;

    bool set = false;
    float minX = 0, minY = 0, maxX = 0, maxY = 0;

    for (const auto& pl : m_polys)
    {
        for (const auto& p : pl.pts)
        {
            if (!set)
            {
                minX = maxX = p.x;
                minY = maxY = p.y;
                set = true;
            }
            else
            {
                minX = std::min(minX, p.x);
                minY = std::min(minY, p.y);
                maxX = std::max(maxX, p.x);
                maxY = std::max(maxY, p.y);
            }
        }
    }

    if (set)
    {
        m_hasBounds = true;
        m_minX = minX;
        m_minY = minY;
        m_maxX = maxX;
        m_maxY = maxY;
    }
}

void CErksMapPreviewWnd::EnsureViewInitialized()
{
    if (m_viewInit)
        return;
    ResetViewToFit();
    m_viewInit = true;
}

void CErksMapPreviewWnd::ResetViewToFit()
{
    CRect rc;
    GetClientRect(&rc);

    const float w = (float)rc.Width() - (kMargin * 2.0f);
    const float h = (float)rc.Height() - (kMargin * 2.0f);

    const float dx = m_maxX - m_minX;
    const float dy = m_maxY - m_minY;

    const float sx = (dx > 0.0f) ? (w / dx) : 1.0f;
    const float sy = (dy > 0.0f) ? (h / dy) : 1.0f;

    m_viewScale = std::max(kMinZoom, std::min(kMaxZoom, std::min(sx, sy)));

    const float cx = ((float)rc.left) + kMargin + ((w - (dx * m_viewScale)) * 0.5f);
    const float cy = ((float)rc.top) + kMargin + ((h - (dy * m_viewScale)) * 0.5f);

    m_viewOffX = cx - (m_minX * m_viewScale);
    m_viewOffY = cy + (m_maxY * m_viewScale);
}

void CErksMapPreviewWnd::ResetViewHome()
{
    if (!m_hasBounds || m_polys.empty())
        return;

    ResetViewToFit();
    m_wheelRemainder = 0.0f;

    Invalidate(FALSE);
}

void CErksMapPreviewWnd::ApplyWheelZoom(float wheelDelta, const CPoint& clientPt)
{
    if (!m_hasBounds || m_polys.empty())
        return;

    EnsureViewInitialized();

    CRect rc;
    GetClientRect(&rc);

    // Keep interaction out of the title/header zone.
    if (clientPt.y < (int)(kMargin))
        return;

    const float step = 1.15f;

    // Accumulate sub-detent deltas to support trackpads/high-resolution wheels.
    // Apply zoom incrementally so recent deltas don't keep re-applying (stability).
    m_wheelRemainder += wheelDelta;

    auto applyDetents = [&](float detents)
    {
        const PointF worldBefore = ClientToWorld(clientPt, rc);

        const float factor = std::pow(step, detents);

        float newScale = m_viewScale * factor;
        newScale = std::max(kMinZoom, std::min(kMaxZoom, newScale));

        if (std::abs(newScale - m_viewScale) < 1e-6f)
            return;

        m_viewScale = newScale;
        m_viewOffX = (float)clientPt.x - (worldBefore.x * m_viewScale);
        m_viewOffY = (float)clientPt.y + (worldBefore.y * m_viewScale);
    };

    const float detentsTotal = m_wheelRemainder / (float)WHEEL_DELTA;

    if (std::abs(detentsTotal) >= 1.0f)
    {
        const float whole = (detentsTotal > 0.0f) ? std::floor(detentsTotal) : std::ceil(detentsTotal);
        applyDetents(whole);
        m_wheelRemainder -= whole * (float)WHEEL_DELTA;
    }
    else
    {
        // For trackpad-style fractional deltas, apply smoothly.
        applyDetents(detentsTotal);
        m_wheelRemainder = 0.0f;
    }

    // Keep remainder bounded to avoid floating point drift.
    if (std::abs(m_wheelRemainder) > 10.0f * (float)WHEEL_DELTA)
        m_wheelRemainder = std::fmod(m_wheelRemainder, (float)WHEEL_DELTA);

    Invalidate(FALSE);
}

BOOL CErksMapPreviewWnd::OnEraseBkgnd(CDC* pDC)
{
    UNREFERENCED_PARAMETER(pDC);
    return TRUE;
}

void CErksMapPreviewWnd::OnPaint()
{
    CPaintDC paintDc(this);

    CRect rc;
    GetClientRect(&rc);

    // Double-buffer
    CDC memDc;
    memDc.CreateCompatibleDC(&paintDc);

    CBitmap backBmp;
    backBmp.CreateCompatibleBitmap(&paintDc, rc.Width(), rc.Height());
    CBitmap* oldBmp = memDc.SelectObject(&backBmp);

    memDc.SetBkMode(TRANSPARENT);
    memDc.FillSolidRect(&rc, m_backColor);

    // Define a clean plot area so UI/labels can live outside.
    CRect plotRc = rc;
    plotRc.DeflateRect((int)kMargin, (int)kMargin);

    // Subtle grid background (AutoCAD-like adaptive grid)
    {
        const COLORREF minor = RGB(40, 40, 40);
        const COLORREF major = RGB(55, 55, 55);

        CPen minorPen(PS_SOLID, 1, minor);
        CPen majorPen(PS_SOLID, 1, major);

        auto clampi = [](int v, int lo, int hi) { return (v < lo) ? lo : (v > hi ? hi : v); };

        // Pick a world step so minor lines are roughly this many pixels apart.
        // AutoCAD-style uses 1/2/5*10^n steps.
        const float targetMinorPx = 24.0f;
        float minorWorldStep = 0.0f;

        if (m_viewInit && m_viewScale > 0.0f)
        {
            const float desiredWorld = targetMinorPx / m_viewScale;
            if (desiredWorld > 0.0f)
            {
                const float decade = std::pow(10.0f, std::floor(std::log10(desiredWorld)));
                const float norm = desiredWorld / decade; // [1..10)

                float base = 1.0f;
                if (norm <= 1.5f) base = 1.0f;
                else if (norm <= 3.5f) base = 2.0f;
                else if (norm <= 7.5f) base = 5.0f;
                else base = 10.0f;

                minorWorldStep = base * decade;
            }
        }

        // Fallback when no view yet (startup): screen-space grid.
        if (!(minorWorldStep > 0.0f))
        {
            if (m_showMinorGrid || m_showMajorGrid)
            {
                const int minorStep = 32;
                const int majorStep = 160;

                for (int x = plotRc.left; x <= plotRc.right; x += minorStep)
                {
                    const bool isMajor = ((x - plotRc.left) % majorStep) == 0;
                    if ((isMajor && !m_showMajorGrid) || (!isMajor && !m_showMinorGrid))
                        continue;

                    CPen* oldPen = memDc.SelectObject(isMajor ? &majorPen : &minorPen);
                    memDc.MoveTo(x, plotRc.top);
                    memDc.LineTo(x, plotRc.bottom);
                    memDc.SelectObject(oldPen);
                }

                for (int y = plotRc.top; y <= plotRc.bottom; y += minorStep)
                {
                    const bool isMajor = ((y - plotRc.top) % majorStep) == 0;
                    if ((isMajor && !m_showMajorGrid) || (!isMajor && !m_showMinorGrid))
                        continue;

                    CPen* oldPen = memDc.SelectObject(isMajor ? &majorPen : &minorPen);
                    memDc.MoveTo(plotRc.left, y);
                    memDc.LineTo(plotRc.right, y);
                    memDc.SelectObject(oldPen);
                }
            }
        }
        else
        {
            // Draw world-aligned grid: major every 5 minors.
            const PointF wTL = ClientToWorld(CPoint(plotRc.left, plotRc.top), rc);
            const PointF wBR = ClientToWorld(CPoint(plotRc.right, plotRc.bottom), rc);

            const float worldMinX = std::min(wTL.x, wBR.x);
            const float worldMaxX = std::max(wTL.x, wBR.x);
            const float worldMinY = std::min(wTL.y, wBR.y);
            const float worldMaxY = std::max(wTL.y, wBR.y);

            // Compute pixel-step for minor grid, then draw by pixels to keep it visually stable.
            const float minorPxStepF = std::max(2.0f, minorWorldStep * m_viewScale);
            const int minorPxStep = (int)std::lround(minorPxStepF);
            const int majorPxStep = minorPxStep * 5;

            // Anchor to world origin -> convert to client pixels for stable start.
            const float anchorX = 0.0f;
            const float anchorY = 0.0f;

            const int anchorXPx = (int)std::lround(m_viewOffX + (anchorX * m_viewScale));
            const int anchorYPx = (int)std::lround(m_viewOffY - (anchorY * m_viewScale));

            auto modPos = [](int a, int m) {
                if (m <= 0) return 0;
                int r = a % m;
                return (r < 0) ? (r + m) : r;
            };

            const int startXPx = plotRc.left - modPos(plotRc.left - anchorXPx, minorPxStep);
            const int startYPx = plotRc.top - modPos(plotRc.top - anchorYPx, minorPxStep);

            if (m_showMinorGrid || m_showMajorGrid)
            {
                for (int x = startXPx; x <= plotRc.right; x += minorPxStep)
                {
                    const bool isMajor = (majorPxStep > 0) ? (modPos(x - anchorXPx, majorPxStep) == 0) : false;
                    if ((isMajor && !m_showMajorGrid) || (!isMajor && !m_showMinorGrid))
                        continue;

                    CPen* oldPen = memDc.SelectObject(isMajor ? &majorPen : &minorPen);
                    memDc.MoveTo(x, plotRc.top);
                    memDc.LineTo(x, plotRc.bottom);
                    memDc.SelectObject(oldPen);
                }

                for (int y = startYPx; y <= plotRc.bottom; y += minorPxStep)
                {
                    const bool isMajor = (majorPxStep > 0) ? (modPos(y - anchorYPx, majorPxStep) == 0) : false;
                    if ((isMajor && !m_showMajorGrid) || (!isMajor && !m_showMinorGrid))
                        continue;

                    CPen* oldPen = memDc.SelectObject(isMajor ? &majorPen : &minorPen);
                    memDc.MoveTo(plotRc.left, y);
                    memDc.LineTo(plotRc.right, y);
                    memDc.SelectObject(oldPen);
                }
            }

            // Scale bar
            {
                // Target visual size in pixels.
                const double targetPx = 140.0;
                const double minPx = 70.0;
                const double maxPx = 220.0;

                const double pxPerWorld = (m_viewScale > 0.0f) ? (double)m_viewScale : 1.0;
                double worldLen = targetPx / pxPerWorld; // in world units (assumed meters)
                if (worldLen <= 0.0)
                    worldLen = 1.0;

                // Pick 1/2/5 * 10^n
                const double decade = std::pow(10.0, std::floor(std::log10(worldLen)));
                const double norm = worldLen / decade;
                double base = 1.0;
                if (norm <= 1.5) base = 1.0;
                else if (norm <= 3.5) base = 2.0;
                else if (norm <= 7.5) base = 5.0;
                else base = 10.0;

                worldLen = base * decade;

                // Clamp to pixel band
                double pxLen = worldLen * pxPerWorld;
                if (pxLen < minPx)
                {
                    worldLen *= 2.0;
                    pxLen = worldLen * pxPerWorld;
                }
                if (pxLen > maxPx)
                {
                    worldLen *= 0.5;
                    pxLen = worldLen * pxPerWorld;
                }

                const int pad = 10;
                const int tickH = 10;
                const int x0 = plotRc.left + pad;
                const int y0 = plotRc.bottom - pad;
                const int x1 = x0 + (int)std::lround(pxLen);

                if (x1 < plotRc.right - pad && (plotRc.Height() > 40))
                {
                    CPen barPen(PS_SOLID, 2, RGB(200, 200, 200));
                    CPen* oldPen = memDc.SelectObject(&barPen);

                    memDc.MoveTo(x0, y0);
                    memDc.LineTo(x1, y0);

                    memDc.MoveTo(x0, y0);
                    memDc.LineTo(x0, y0 - tickH);
                    memDc.MoveTo(x1, y0);
                    memDc.LineTo(x1, y0 - tickH);

                    const int xm = (x0 + x1) / 2;
                    memDc.MoveTo(xm, y0);
                    memDc.LineTo(xm, y0 - (tickH - 3));

                    memDc.SelectObject(oldPen);

                    // Label text
                    auto fmtMeters = [](double m) -> CString {
                        if (m >= 1000.0)
                        {
                            const double km = m / 1000.0;
                            wchar_t buf[64];
                            if (km >= 10.0)
                                _snwprintf_s(buf, _countof(buf), _TRUNCATE, L"%.0f km", km);
                            else
                                _snwprintf_s(buf, _countof(buf), _TRUNCATE, L"%.1f km", km);
                            return CString(buf);
                        }
                        const long long iv = (long long)std::llround(m);
                        return CString((std::to_wstring(iv) + L" m").c_str());
                    };

                    CFont* labelFont = GetParent() ? GetParent()->GetFont() : nullptr;
                    CFont* oldF = labelFont ? memDc.SelectObject(labelFont) : nullptr;
                    memDc.SetTextColor(RGB(210, 210, 210));

                    const CString lbl = fmtMeters(worldLen);
                    CRect tRc(x0, y0 - tickH - 18, x1, y0 - tickH - 2);
                    memDc.DrawText(lbl, -1, &tRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

                    if (oldF)
                        memDc.SelectObject(oldF);
                }
            }
        }

        // Crosshair at center (scaled by zoom)
        const int cx = rc.left + rc.Width() / 2;
        const int cy = rc.top + rc.Height() / 2;

        int halfLen = 10;
        if (m_viewInit)
        {
            const float s = m_viewScale;
            halfLen = (int)std::lround(std::max(6.0f, std::min(28.0f, 10.0f * std::sqrt(std::max(0.001f, s)))));
        }

        CPen crossPen(PS_SOLID, 1, RGB(75, 75, 75));
        CPen* oldPen = memDc.SelectObject(&crossPen);
        memDc.MoveTo(cx - halfLen, cy);
        memDc.LineTo(cx + halfLen, cy);
        memDc.MoveTo(cx, cy - halfLen);
        memDc.LineTo(cx, cy + halfLen);
        memDc.SelectObject(oldPen);
    }

    // Border
    {
        CPen borderPen(PS_SOLID, 1, m_borderColor);
        CPen* oldPen = memDc.SelectObject(&borderPen);
        HBRUSH oldBrush = (HBRUSH)memDc.SelectStockObject(NULL_BRUSH);
        memDc.Rectangle(&rc);
        memDc.SelectObject(oldBrush);
        memDc.SelectObject(oldPen);
    }

    // Title (kept as GDI text)
    CFont* oldFont = memDc.SelectObject(GetParent() ? GetParent()->GetFont() : nullptr);

    CRect titleRc = rc;
    titleRc.DeflateRect(12, 10);
    memDc.SetTextColor(m_textColor);
    memDc.DrawText(_T("Map Preview"), -1, &titleRc, DT_LEFT | DT_TOP | DT_SINGLELINE);

    if (!m_hasBounds || m_polys.empty())
    {
        CRect hintRc = rc;
        hintRc.DeflateRect(12, 34);
        memDc.SetTextColor(RGB(140, 140, 140));
        memDc.DrawText(_T("Open Existing Data (*.dwg) to load minor/major layers"), -1, &hintRc, DT_LEFT | DT_TOP | DT_SINGLELINE);
    }
    else
    {
        EnsureViewInitialized();

        // Draw polylines
        for (const auto& pl : m_polys)
        {
            if (pl.pts.size() < 2)
                continue;

            CPen pen(PS_SOLID, 1, pl.color);
            CPen* pOld = memDc.SelectObject(&pen);

            CPoint p0 = WorldToClient(pl.pts[0], rc);
            memDc.MoveTo(p0);

            for (size_t i = 1; i < pl.pts.size(); ++i)
            {
                CPoint pi = WorldToClient(pl.pts[i], rc);
                memDc.LineTo(pi);
            }

            if (pl.closed)
            {
                CPoint pStart = WorldToClient(pl.pts[0], rc);
                memDc.LineTo(pStart);
            }

            memDc.SelectObject(pOld);
        }
    }

    if (oldFont)
        memDc.SelectObject(oldFont);

    paintDc.BitBlt(0, 0, rc.Width(), rc.Height(), &memDc, 0, 0, SRCCOPY);
    memDc.SelectObject(oldBmp);
}

BOOL CErksMapPreviewWnd::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
    UNREFERENCED_PARAMETER(nFlags);

    if (!m_hasBounds || m_polys.empty())
        return FALSE;

    CPoint clientPt = pt;
    ScreenToClient(&clientPt);

    ApplyWheelZoom((float)zDelta, clientPt);
    return TRUE;
}

void CErksMapPreviewWnd::OnMButtonDown(UINT nFlags, CPoint point)
{
    UNREFERENCED_PARAMETER(nFlags);

    if (!m_hasBounds || m_polys.empty())
        return;

    EnsureViewInitialized();

    m_panning = true;
    m_panStartPt = point;
    m_panStartOffX = m_viewOffX;
    m_panStartOffY = m_viewOffY;

    SetCapture();
    SetFocus();
}

void CErksMapPreviewWnd::OnMButtonUp(UINT nFlags, CPoint point)
{
    UNREFERENCED_PARAMETER(nFlags);
    UNREFERENCED_PARAMETER(point);

    if (m_panning)
    {
        m_panning = false;
        if (GetCapture() == this)
            ReleaseCapture();
    }
}

void CErksMapPreviewWnd::OnMouseMove(UINT nFlags, CPoint point)
{
    UNREFERENCED_PARAMETER(nFlags);

    if (!m_panning)
        return;

    const CPoint delta = point - m_panStartPt;
    m_viewOffX = m_panStartOffX + (float)delta.x;
    m_viewOffY = m_panStartOffY + (float)delta.y;

    // Snap to pixel to avoid jitter of grid lines due to rounding.
    m_viewOffX = (float)std::lround(m_viewOffX);
    m_viewOffY = (float)std::lround(m_viewOffY);

    Invalidate(FALSE);
}

void CErksMapPreviewWnd::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
    UNREFERENCED_PARAMETER(nRepCnt);

    switch (nChar)
    {
    case 'F':
        ResetViewToFit();
        m_viewInit = true;
        m_wheelRemainder = 0.0f;
        Invalidate(FALSE);
        break;
    case VK_HOME:
        ResetViewHome();
        break;
    case '1':
        m_showMinorGrid = !m_showMinorGrid;
        Invalidate(FALSE);
        break;
    case '2':
        m_showMajorGrid = !m_showMajorGrid;
        Invalidate(FALSE);
        break;
    default:
        break;
    }

    CWnd::OnKeyDown(nChar, nRepCnt, nFlags);
}

void CErksMapPreviewWnd::OnContextMenu(CWnd* pWnd, CPoint point)
{
    UNREFERENCED_PARAMETER(pWnd);

    CMenu menu;
    if (!menu.CreatePopupMenu())
        return;

    enum
    {
        ID_FIT = 1001,
        ID_HOME = 1002,
        ID_TOGGLE_MINOR = 1101,
        ID_TOGGLE_MAJOR = 1102,
    };

    menu.AppendMenu(MF_STRING, ID_FIT, _T("Fit to Extents\tF"));
    menu.AppendMenu(MF_STRING, ID_HOME, _T("Reset View\tHome"));
    menu.AppendMenu(MF_SEPARATOR);
    menu.AppendMenu(MF_STRING | (m_showMinorGrid ? MF_CHECKED : 0), ID_TOGGLE_MINOR, _T("Show Minor Grid\t1"));
    menu.AppendMenu(MF_STRING | (m_showMajorGrid ? MF_CHECKED : 0), ID_TOGGLE_MAJOR, _T("Show Major Grid\t2"));

    const UINT cmd = menu.TrackPopupMenu(TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y, this);
    if (cmd == 0)
        return;

    switch (cmd)
    {
    case ID_FIT:
        ResetViewToFit();
        m_viewInit = true;
        m_wheelRemainder = 0.0f;
        Invalidate(FALSE);
        break;
    case ID_HOME:
        ResetViewHome();
        break;
    case ID_TOGGLE_MINOR:
        m_showMinorGrid = !m_showMinorGrid;
        Invalidate(FALSE);
        break;
    case ID_TOGGLE_MAJOR:
        m_showMajorGrid = !m_showMajorGrid;
        Invalidate(FALSE);
        break;
    default:
        break;
    }
}

void CErksMapPreviewWnd::OnLButtonDblClk(UINT nFlags, CPoint point)
{
    UNREFERENCED_PARAMETER(nFlags);
    UNREFERENCED_PARAMETER(point);

    if (!m_hasBounds || m_polys.empty())
        return;

    ResetViewToFit();
    m_viewInit = true;
    m_wheelRemainder = 0.0f;
    Invalidate(FALSE);
}

CErksMapPreviewWnd::PointF CErksMapPreviewWnd::ClientToWorld(const CPoint& ptClient, const CRect& rcClient) const
{
    UNREFERENCED_PARAMETER(rcClient);
    const float wx = ((float)ptClient.x - m_viewOffX) / m_viewScale;
    const float wy = (m_viewOffY - (float)ptClient.y) / m_viewScale;
    return PointF(wx, wy);
}

CPoint CErksMapPreviewWnd::WorldToClient(const PointF& p, const CRect& rcClient) const
{
    UNREFERENCED_PARAMETER(rcClient);
    const float x = m_viewOffX + (p.x * m_viewScale);
    const float y = m_viewOffY - (p.y * m_viewScale);
    return CPoint((int)std::lround(x), (int)std::lround(y));
}
