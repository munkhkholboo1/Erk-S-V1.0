#include "StdAfx.h"
#include "ErksPopupMenuWnd.h"

BEGIN_MESSAGE_MAP(CErksPopupMenuWnd, CWnd)
    ON_WM_PAINT()
    ON_WM_MOUSEMOVE()
    ON_WM_LBUTTONDOWN()
    ON_WM_LBUTTONUP()
    ON_WM_KILLFOCUS()
    ON_WM_KEYDOWN()
    ON_WM_CAPTURECHANGED()
END_MESSAGE_MAP()

static int TextWidth(CDC& dc, const std::wstring& s)
{
    if (s.empty())
        return 0;
    CSize sz = dc.GetTextExtent(s.c_str());
    return sz.cx;
}

UINT CErksPopupMenuWnd::Track(CWnd* owner, const CPoint& screenPt, const std::vector<ErksPopupMenuItem>& items,
    COLORREF backColor, COLORREF textColor, COLORREF hotBackColor, COLORREF borderColor)
{
    m_items = items;
    m_backColor = backColor;
    m_textColor = textColor;
    m_hotBackColor = hotBackColor;
    m_borderColor = borderColor;

    m_hover = -1;
    m_result = 0;

    CString cls = AfxRegisterWndClass(CS_DBLCLKS, ::LoadCursor(nullptr, IDC_ARROW), (HBRUSH)GetStockObject(NULL_BRUSH), nullptr);

    // Prepare a font slightly smaller than the owner font to match typical menu sizing.
    static CFont s_popupFont;
    static HFONT s_lastOwnerFont = nullptr;

    // Use the exact same font as the owner to match File/About.
    if (owner && owner->GetFont())
    {
        if (s_popupFont.GetSafeHandle())
            s_popupFont.DeleteObject();
        LOGFONT lf{};
        HFONT hOwnerFont = (HFONT)owner->GetFont()->GetSafeHandle();
        if (hOwnerFont && ::GetObject(hOwnerFont, sizeof(lf), &lf) == sizeof(lf))
        {
            lf.lfWeight = FW_NORMAL;
            s_popupFont.CreateFontIndirect(&lf);
        }
    }

    // Pick font for measuring.
    CFont* fontToUse = (s_popupFont.GetSafeHandle() != nullptr) ? &s_popupFont : (owner ? owner->GetFont() : nullptr);

    CClientDC dc(owner);
    CFont* oldFont = nullptr;
    if (fontToUse)
        oldFont = dc.SelectObject(fontToUse);
    else
        oldFont = (CFont*)dc.SelectStockObject(DEFAULT_GUI_FONT);

    const int w = CalcWidth(dc);

    TEXTMETRIC tm{};
    dc.GetTextMetrics(&tm);
    dc.SelectObject(oldFont);

    const int fontH = tm.tmHeight;
    m_itemHeight = max(18, fontH + 4);
    m_sepHeight = max(8, (fontH / 2) + 4);

    const int h = TotalHeight();

    DWORD style = WS_POPUP;
    DWORD exStyle = WS_EX_TOOLWINDOW | WS_EX_TOPMOST;

    if (!CreateEx(exStyle, cls, _T("ERKS_POPUP_MENU"), style, CRect(screenPt.x, screenPt.y, screenPt.x + w, screenPt.y + h), owner, 0))
        return 0;

    // Apply the font after window creation so WM_SETFONT reaches the window.
    if (fontToUse)
        SetFont(fontToUse, FALSE);

    ShowWindow(SW_SHOWNOACTIVATE);
    UpdateWindow();

    // Avoid SetFocus/SetCapture: AutoCAD can steal focus/capture immediately.

    m_running = true;

    MSG msg;
    while (m_running && ::GetMessage(&msg, nullptr, 0, 0))
    {
        // Close when user clicks outside the popup.
        if (msg.message == WM_LBUTTONDOWN || msg.message == WM_RBUTTONDOWN)
        {
            POINT pt{ GET_X_LPARAM(msg.lParam), GET_Y_LPARAM(msg.lParam) };
            // For non-client/global messages lParam may not be screen; use cursor position.
            ::GetCursorPos(&pt);
            HWND h = ::WindowFromPoint(pt);
            if (h != GetSafeHwnd() && !::IsChild(GetSafeHwnd(), h))
            {
                End(0);
                continue;
            }
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (GetSafeHwnd())
        DestroyWindow();

    return m_result;
}

void CErksPopupMenuWnd::End(UINT cmd)
{
    m_result = cmd;
    m_running = false;
    PostMessage(WM_NULL);
}

int CErksPopupMenuWnd::ItemHeight(int index) const
{
    if (index < 0 || index >= (int)m_items.size())
        return 0;
    return (m_items[index].id == 0) ? m_sepHeight : m_itemHeight;
}

int CErksPopupMenuWnd::TotalHeight() const
{
    int h = 2; // border
    for (int i = 0; i < (int)m_items.size(); ++i)
        h += ItemHeight(i) + m_gapY;
    return h;
}

int CErksPopupMenuWnd::CalcWidth(CDC& dc) const
{
    int maxText = 0;
    for (auto& it : m_items)
        maxText = max(maxText, TextWidth(dc, it.text));

    // left+right padding plus some room
    int w = (m_paddingX * 2) + maxText + 24;
    if (w < 180)
        w = 180;
    return w + 2; // border
}

CRect CErksPopupMenuWnd::ItemRect(int index) const
{
    CRect rc;
    GetClientRect(&rc);

    int y = 1; // border
    for (int i = 0; i < index; ++i)
        y += ItemHeight(i) + m_gapY;

    const int h = ItemHeight(index);
    return CRect(1, y, rc.right - 1, y + h);
}

int CErksPopupMenuWnd::HitTest(const CPoint& pt) const
{
    for (int i = 0; i < (int)m_items.size(); ++i)
    {
        CRect r = ItemRect(i);
        if (r.PtInRect(pt))
        {
            if (m_items[i].id == 0)
                return -1;
            if (!m_items[i].enabled)
                return -1;
            return i;
        }
    }
    return -1;
}

void CErksPopupMenuWnd::OnPaint()
{
    CPaintDC dc(this);

    CRect rc;
    GetClientRect(&rc);

    // background
    dc.FillSolidRect(&rc, m_backColor);

    // 1px border
    CPen borderPen(PS_SOLID, 1, m_borderColor);
    CPen* oldPen = dc.SelectObject(&borderPen);
    HBRUSH oldBrush = (HBRUSH)dc.SelectStockObject(NULL_BRUSH);
    dc.Rectangle(&rc);
    dc.SelectObject(oldBrush);

    CFont* oldFont = dc.SelectObject(GetFont());
    dc.SetBkMode(TRANSPARENT);

    for (int i = 0; i < (int)m_items.size(); ++i)
    {
        CRect r = ItemRect(i);
        const auto& it = m_items[i];

        if (it.id == 0)
        {
            const int y = r.top + (r.Height() / 2);
            CPen sepPen(PS_SOLID, 1, m_borderColor);
            CPen* pOld = dc.SelectObject(&sepPen);
            dc.MoveTo(r.left + m_paddingX, y);
            dc.LineTo(r.right - m_paddingX, y);
            dc.SelectObject(pOld);
            continue;
        }

        const bool hot = (i == m_hover);
        const COLORREF back = hot ? m_hotBackColor : m_backColor;
        dc.FillSolidRect(&r, back);

        const COLORREF txt = it.enabled ? m_textColor : RGB(140, 140, 140);
        dc.SetTextColor(txt);

        CRect tr = r;
        tr.DeflateRect(m_paddingX, 0);
        dc.DrawText(it.text.c_str(), (int)it.text.size(), &tr, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
    }

    dc.SelectObject(oldFont);
    dc.SelectObject(oldPen);
}

void CErksPopupMenuWnd::OnMouseMove(UINT nFlags, CPoint point)
{
    UNREFERENCED_PARAMETER(nFlags);

    const int hit = HitTest(point);
    if (hit != m_hover)
    {
        m_hover = hit;
        Invalidate(FALSE);
    }
}

void CErksPopupMenuWnd::OnLButtonDown(UINT nFlags, CPoint point)
{
    UNREFERENCED_PARAMETER(nFlags);
    UNREFERENCED_PARAMETER(point);
}

void CErksPopupMenuWnd::OnLButtonUp(UINT nFlags, CPoint point)
{
    UNREFERENCED_PARAMETER(nFlags);

    int hit = HitTest(point);
    if (hit >= 0)
        End(m_items[hit].id);
    else
        End(0);
}

void CErksPopupMenuWnd::OnKillFocus(CWnd* pNewWnd)
{
    // Do not auto-close on focus changes; AutoCAD can steal focus transiently.
    CWnd::OnKillFocus(pNewWnd);
}

void CErksPopupMenuWnd::OnCaptureChanged(CWnd* pWnd)
{
    // AutoCAD can change mouse capture while showing popups; do not treat as cancellation.
    CWnd::OnCaptureChanged(pWnd);
}

void CErksPopupMenuWnd::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
    UNREFERENCED_PARAMETER(nRepCnt);
    UNREFERENCED_PARAMETER(nFlags);

    if (nChar == VK_ESCAPE)
    {
        End(0);
        return;
    }

    if (nChar == VK_RETURN || nChar == VK_SPACE)
    {
        if (m_hover >= 0 && m_hover < (int)m_items.size())
            End(m_items[m_hover].id);
        return;
    }

    if (nChar == VK_DOWN || nChar == VK_UP)
    {
        const int dir = (nChar == VK_DOWN) ? 1 : -1;
        int idx = m_hover;
        for (int hop = 0; hop < (int)m_items.size(); ++hop)
        {
            idx += dir;
            if (idx < 0) idx = (int)m_items.size() - 1;
            if (idx >= (int)m_items.size()) idx = 0;
            if (m_items[idx].id != 0 && m_items[idx].enabled)
            {
                m_hover = idx;
                Invalidate(FALSE);
                break;
            }
        }
        return;
    }

    CWnd::OnKeyDown(nChar, nRepCnt, nFlags);
}
