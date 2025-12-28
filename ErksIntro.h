#pragma once

#include <afxwin.h>

class CErksIntro : public CWnd
{
public:
    CErksIntro();
    ~CErksIntro() override;

    bool Show(int width, int height, DWORD durationMs, HWND notifyHwnd = nullptr, UINT notifyMsg = 0, WPARAM notifyWParam = 0, LPARAM notifyLParam = 0);

protected:
    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnDestroy();
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg void OnPaint();
    virtual void PostNcDestroy() override;

    DECLARE_MESSAGE_MAP()

private:
    int m_width = 800;
    int m_height = 450;
    UINT_PTR m_closeTimer = 0;
    DWORD m_durationMs = 3000;

    HBITMAP m_bitmap = nullptr;

    void CloseNow();
    bool LoadIntroBitmap();

    HWND m_notifyHwnd = nullptr;
    UINT m_notifyMsg = 0;
    WPARAM m_notifyWParam = 0;
    LPARAM m_notifyLParam = 0;
};
