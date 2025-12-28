#include "StdAfx.h"
#include "ErksUiController.h"

#include "ErksIntro.h"
#include "ErksMainDialog.h"

namespace {
    constexpr UINT WM_ERKS_INTRO_CLOSED = WM_APP + 0x2A11;
}

BEGIN_MESSAGE_MAP(CErksUiController, CWnd)
    ON_MESSAGE(WM_ERKS_INTRO_CLOSED, &CErksUiController::OnIntroClosed)
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

    // Start intro (self-deletes). When it closes, we receive WM_ERKS_INTRO_CLOSED.
    CErksIntro* intro = new CErksIntro();
    intro->Show(960, 540, 2000, GetSafeHwnd(), WM_ERKS_INTRO_CLOSED, 0, (LPARAM)pParent);
}

LRESULT CErksUiController::OnIntroClosed(WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(wParam);

    CWnd* parent = (CWnd*)lParam;
    ShowUi(parent);
    return 0;
}

void CErksUiController::ShowUi(CWnd* pParent)
{
    if (m_dlg && ::IsWindow(m_dlg->GetSafeHwnd()))
    {
        m_dlg->SetForegroundWindow();
        return;
    }

    // Create as a top-level tool window instead of parenting to AutoCAD frame;
    // parenting can cause AutoCAD to suppress sizing and min/max behaviors.
    UNREFERENCED_PARAMETER(pParent);

    m_dlg = new CErksMainDialog(nullptr);
    if (!m_dlg->Create(CErksMainDialog::IDD, nullptr))
    {
        delete m_dlg;
        m_dlg = nullptr;
        return;
    }

    m_dlg->ShowWindow(SW_SHOW);
    m_dlg->SetForegroundWindow();
}

void CErksUiController::ClearDialog()
{
    m_dlg = nullptr;
}
