#pragma once

#include <afxwin.h>
#include <aduiDialog.h>
#include <unordered_map>
#include <string>
#include "Resource.h"

class CErksMainDialog : public CAdUiDialog
{
public:
    explicit CErksMainDialog(CWnd* pParent = nullptr);
    enum { IDD = IDD_ERKS_MAIN };

protected:
    virtual BOOL OnInitDialog() override;
    virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam) override;
    virtual void PostNcDestroy() override;
    virtual void OnOK() override;
    virtual void OnCancel() override;

    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
    afx_msg void OnMeasureItem(int nIDCtl, LPMEASUREITEMSTRUCT lpMeasureItemStruct);
    afx_msg void OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct);
    afx_msg void OnInitMenuPopup(CMenu* pPopupMenu, UINT nIndex, BOOL bSysMenu);
    afx_msg void OnClose();
    afx_msg void OnGetMinMaxInfo(MINMAXINFO* lpMMI);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg LRESULT OnNcHitTest(CPoint point);
    afx_msg void OnSysCommand(UINT nID, LPARAM lParam);

    DECLARE_MESSAGE_MAP()

private:
    CBrush m_darkBrush;
    CMenu m_mainMenu;
    COLORREF m_backColor = RGB(32, 32, 32);
    COLORREF m_textColor = RGB(230, 230, 230);
    COLORREF m_hotBackColor = RGB(45, 45, 48);
    COLORREF m_borderColor = RGB(60, 60, 60);

    std::unordered_map<UINT, std::wstring> m_menuTextById;

    void CaptureMenuText(CMenu* menu);
    void ApplyOwnerDrawToMenu(CMenu* menu);

    void OnFileOpen();
    void OnFileSave();
    void OnFileSaveAs();
    void OnFileOpenExistingData();
    void OnFileCreateRoadAxis();
    void OnFileExit();
    void OnHelpAbout();
};
