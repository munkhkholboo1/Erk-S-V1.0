#pragma once

#include <afxwin.h>
#include <vector>

class CErksMapPreviewWnd : public CWnd
{
public:
    struct PointF
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        PointF() = default;
        PointF(float xx, float yy) : x(xx), y(yy), z(0.0f) {}
        PointF(float xx, float yy, float zz) : x(xx), y(yy), z(zz) {}
    };

    struct Polyline2d
    {
        std::vector<PointF> pts; // world XYZ (Z used for labels)
        COLORREF color = RGB(230, 230, 230);
        bool closed = false;
    };

    BOOL Create(CWnd* pParent);

    void SetTheme(COLORREF backColor, COLORREF borderColor, COLORREF textColor);
    void SetGeometry(const std::vector<Polyline2d>& polys);
    void SetReloadTarget(CWnd* target, UINT commandId);
    void SetReloadEnabled(bool enabled);

protected:
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
    afx_msg void OnMButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnMButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
    afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
    afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnReloadOverlayClicked();
    afx_msg void OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct);

    DECLARE_MESSAGE_MAP()

private:
    void ComputeWorldBounds();
    void EnsureViewInitialized();
    void ResetViewToFit();
    void ResetViewHome();
    void ApplyWheelZoom(float wheelDelta, const CPoint& clientPt);
    PointF ClientToWorld(const CPoint& ptClient, const CRect& rcClient) const;
    CPoint WorldToClient(const PointF& p, const CRect& rcClient) const;
    void LayoutOverlay();
    void DrawReloadGlyph(CDC& dc, const CRect& rc, COLORREF color);

private:
    // Theme
    COLORREF m_backColor = RGB(32, 32, 32);
    COLORREF m_borderColor = RGB(60, 60, 60);
    COLORREF m_textColor = RGB(230, 230, 230);

    // Data
    std::vector<Polyline2d> m_polys;
    bool m_hasBounds = false;
    float m_minX = 0.0f;
    float m_minY = 0.0f;
    float m_maxX = 0.0f;
    float m_maxY = 0.0f;

    // View (client-space)
    bool m_viewInit = false;
    float m_viewScale = 1.0f; // pixels per world unit
    float m_viewOffX = 0.0f;  // pixels
    float m_viewOffY = 0.0f;  // pixels

    // Interaction
    bool m_panning = false;
    CPoint m_panStartPt{};
    float m_panStartOffX = 0.0f;
    float m_panStartOffY = 0.0f;

    // Wheel smoothing (trackpad/high-resolution wheels)
    float m_wheelRemainder = 0.0f; // in WHEEL_DELTA units

    // Grid display toggles
    bool m_showMinorGrid = true;
    bool m_showMajorGrid = true;

    // Overlay reload icon button (top-right)
    CButton m_btnReload;
    CWnd* m_reloadTarget = nullptr;
    UINT m_reloadCommandId = 0;

    static constexpr float kMargin = 26.0f;
    static constexpr float kMinZoom = 0.02f;
    static constexpr float kMaxZoom = 200.0f;
    enum { IDC_PREVIEW_RELOAD = 55001 };
};
