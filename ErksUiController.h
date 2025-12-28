#pragma once

#include <afxwin.h>

class CErksMainDialog;

class CErksUiController : public CWnd
{
public:
    static CErksUiController& Instance();

    void ShowIntroThenUi(CWnd* pParent);
    void ClearDialog();

protected:
    afx_msg LRESULT OnIntroClosed(WPARAM wParam, LPARAM lParam);

    DECLARE_MESSAGE_MAP()

private:
    CErksUiController();

    void EnsureCreated();
    void ShowUi(CWnd* pParent);

    CErksMainDialog* m_dlg = nullptr;
};
