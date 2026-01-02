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
        PointF() = default;
        PointF(float xx, float yy) : x(xx), y(yy) {}
    };

    struct Polyline2d
    {
        std::vector<PointF> pts; // world XY
        COLORREF color = RGB(230, 230, 230);
        bool closed = false;
    };

    BOOL Create(CWnd* pParent);

    void SetTheme(COLORREF backColor, COLORREF borderColor, COLORREF textColor);
    void SetGeometry(const std::vector<Polyline2d>& polys);

protected:
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
    afx_msg void OnMButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnMButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);

    DECLARE_MESSAGE_MAP()

private:
    void ComputeWorldBounds();
    void EnsureViewInitialized();
    void ResetViewToFit();
    PointF ClientToWorld(const CPoint& ptClient, const CRect& rcClient) const;
    CPoint WorldToClient(const PointF& p, const CRect& rcClient) const;

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

    static constexpr float kMargin = 26.0f;
    static constexpr float kMinZoom = 0.02f;
    static constexpr float kMaxZoom = 200.0f;
};
