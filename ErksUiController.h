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
    DECLARE_MESSAGE_MAP()

private:
    CErksUiController();

    void EnsureCreated();
    void ShowUi(CWnd* pParent);

    CErksMainDialog* m_dlg = nullptr;
};
