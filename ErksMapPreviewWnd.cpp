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
END_MESSAGE_MAP()

BOOL CErksMapPreviewWnd::Create(CWnd* pParent)
{
    if (!pParent || !::IsWindow(pParent->GetSafeHwnd()))
        return FALSE;

    CString cls = AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW, ::LoadCursor(nullptr, IDC_ARROW), (HBRUSH)GetStockObject(NULL_BRUSH), nullptr);
    return CWnd::CreateEx(0, cls, _T("ERKS_MAP_PREVIEW"), WS_CHILD | WS_VISIBLE, CRect(0, 0, 0, 0), pParent, 0);
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
            const int minorStep = 32;
            const int majorStep = 160;

            for (int x = rc.left; x <= rc.right; x += minorStep)
            {
                const bool isMajor = ((x - rc.left) % majorStep) == 0;
                CPen* oldPen = memDc.SelectObject(isMajor ? &majorPen : &minorPen);
                memDc.MoveTo(x, rc.top);
                memDc.LineTo(x, rc.bottom);
                memDc.SelectObject(oldPen);
            }

            for (int y = rc.top; y <= rc.bottom; y += minorStep)
            {
                const bool isMajor = ((y - rc.top) % majorStep) == 0;
                CPen* oldPen = memDc.SelectObject(isMajor ? &majorPen : &minorPen);
                memDc.MoveTo(rc.left, y);
                memDc.LineTo(rc.right, y);
                memDc.SelectObject(oldPen);
            }
        }
        else
        {
            // Draw world-aligned grid (origin-aligned): major every 5 minors.
            // Determine world viewport extents
            const PointF wTL = ClientToWorld(CPoint(rc.left, rc.top), rc);
            const PointF wBR = ClientToWorld(CPoint(rc.right, rc.bottom), rc);

            const float worldMinX = std::min(wTL.x, wBR.x);
            const float worldMaxX = std::max(wTL.x, wBR.x);
            const float worldMinY = std::min(wTL.y, wBR.y);
            const float worldMaxY = std::max(wTL.y, wBR.y);

            auto floorToStep = [](float v, float step) {
                return std::floor(v / step) * step;
            };

            // Limit line count to protect GDI when zoomed way out.
            const int maxLines = 2000;
            const int xCount = (int)std::ceil((worldMaxX - worldMinX) / minorWorldStep);
            const int yCount = (int)std::ceil((worldMaxY - worldMinY) / minorWorldStep);

            const int safeXCount = clampi(xCount, 0, maxLines);
            const int safeYCount = clampi(yCount, 0, maxLines);

            const float startX = floorToStep(worldMinX, minorWorldStep);
            const float startY = floorToStep(worldMinY, minorWorldStep);

            // Vertical lines
            for (int i = 0; i <= safeXCount; ++i)
            {
                const float xw = startX + (float)i * minorWorldStep;
                const bool isMajor = (i % 5) == 0;

                CPen* oldPen = memDc.SelectObject(isMajor ? &majorPen : &minorPen);
                const CPoint p0 = WorldToClient(PointF(xw, worldMinY), rc);
                const CPoint p1 = WorldToClient(PointF(xw, worldMaxY), rc);
                memDc.MoveTo(p0.x, rc.top);
                memDc.LineTo(p1.x, rc.bottom);
                memDc.SelectObject(oldPen);
            }

            // Horizontal lines
            for (int i = 0; i <= safeYCount; ++i)
            {
                const float yw = startY + (float)i * minorWorldStep;
                const bool isMajor = (i % 5) == 0;

                CPen* oldPen = memDc.SelectObject(isMajor ? &majorPen : &minorPen);
                const CPoint p0 = WorldToClient(PointF(worldMinX, yw), rc);
                const CPoint p1 = WorldToClient(PointF(worldMaxX, yw), rc);
                memDc.MoveTo(rc.left, p0.y);
                memDc.LineTo(rc.right, p1.y);
                memDc.SelectObject(oldPen);
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

    EnsureViewInitialized();

    CPoint clientPt = pt;
    ScreenToClient(&clientPt);

    CRect rc;
    GetClientRect(&rc);

    if (clientPt.y < (int)(kMargin))
        return FALSE;

    const PointF worldBefore = ClientToWorld(clientPt, rc);

    const int detents = zDelta / WHEEL_DELTA;
    if (detents == 0)
        return FALSE;

    const float step = 1.15f;
    float factor = 1.0f;
    if (detents > 0)
    {
        for (int i = 0; i < detents; ++i)
            factor *= step;
    }
    else
    {
        for (int i = 0; i < -detents; ++i)
            factor /= step;
    }

    float newScale = m_viewScale * factor;
    newScale = std::max(kMinZoom, std::min(kMaxZoom, newScale));

    if (std::abs(newScale - m_viewScale) < 1e-6f)
        return TRUE;

    m_viewScale = newScale;
    m_viewOffX = (float)clientPt.x - (worldBefore.x * m_viewScale);
    m_viewOffY = (float)clientPt.y + (worldBefore.y * m_viewScale);

    Invalidate(FALSE);
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

    Invalidate(FALSE);
}
