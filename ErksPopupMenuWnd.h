#pragma once

#include <afxwin.h>
#include <vector>
#include <string>

struct ErksPopupMenuItem
{
    UINT id = 0; // 0 = separator
    std::wstring text;
    bool enabled = true;
};

class CErksPopupMenuWnd : public CWnd
{
public:
    CErksPopupMenuWnd() = default;

    // Shows the popup and blocks (modal loop) until selection or cancel.
    // Returns selected command id, or 0 if canceled.
    UINT Track(CWnd* owner, const CPoint& screenPt, const std::vector<ErksPopupMenuItem>& items,
        COLORREF backColor, COLORREF textColor, COLORREF hotBackColor, COLORREF borderColor);

protected:
    afx_msg void OnPaint();
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnKillFocus(CWnd* pNewWnd);
    afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
    afx_msg void OnCaptureChanged(CWnd* pWnd);

    DECLARE_MESSAGE_MAP()

private:
    void RecalcLayout();
    int HitTest(const CPoint& pt) const;
    CRect ItemRect(int index) const;
    int ItemHeight(int index) const;
    int CalcWidth(CDC& dc) const;
    int TotalHeight() const;

    void End(UINT cmd);

private:
    std::vector<ErksPopupMenuItem> m_items;
    COLORREF m_backColor = RGB(32, 32, 32);
    COLORREF m_textColor = RGB(230, 230, 230);
    COLORREF m_hotBackColor = RGB(45, 45, 48);
    COLORREF m_borderColor = RGB(60, 60, 60);

    int m_hover = -1;
    UINT m_result = 0;
    bool m_running = false;

    int m_paddingX = 12;
    int m_gapY = 0;
    int m_itemHeight = 26;
    int m_sepHeight = 10;
};
