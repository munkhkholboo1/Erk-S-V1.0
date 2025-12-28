#include "StdAfx.h"
#include "ErksUiController.h"

#include "ErksMainDialog.h"

BEGIN_MESSAGE_MAP(CErksUiController, CWnd)
END_MESSAGE_MAP()

CErksUiController& CErksUiController::Instance()
{
    static CErksUiController s;
    return s;
}

CErksUiController::CErksUiController() {}

void CErksUiController::EnsureCreated()
{
    if (GetSafeHwnd())
        return;

    CreateEx(0, AfxRegisterWndClass(0), _T("ERKS_UICONTROLLER"), WS_POPUP, CRect(0, 0, 0, 0), nullptr, 0);
}

void CErksUiController::ShowIntroThenUi(CWnd* pParent)
{
    EnsureCreated();

    if (m_dlg && ::IsWindow(m_dlg->GetSafeHwnd()))
    {
        m_dlg->SetForegroundWindow();
        return;
    }

    ShowUi(pParent);
}

void CErksUiController::ShowUi(CWnd* pParent)
{
    if (m_dlg && ::IsWindow(m_dlg->GetSafeHwnd()))
    {
        m_dlg->SetForegroundWindow();
        return;
    }

    UNREFERENCED_PARAMETER(pParent);

    m_dlg = new CErksMainDialog(nullptr);
    if (!m_dlg->Create(CErksMainDialog::IDD, nullptr))
    {
        delete m_dlg;
        m_dlg = nullptr;
        return;
    }

    m_dlg->RefreshFrameStyles();

    m_dlg->ShowWindow(SW_SHOW);
    m_dlg->SetForegroundWindow();
}

void CErksUiController::ClearDialog()
{
    m_dlg = nullptr;
}
