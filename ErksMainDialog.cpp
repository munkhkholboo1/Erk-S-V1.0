#include "StdAfx.h"
#include "ErksMainDialog.h"

CErksMainDialog::CErksMainDialog(CWnd* pParent)
    : CDialog(CErksMainDialog::IDD, pParent)
{
}

BOOL CErksMainDialog::OnInitDialog()
{
    CDialog::OnInitDialog();

    CString title;
    title.LoadString(IDS_PROJNAME);
    if (!title.IsEmpty())
        SetWindowText(title);

    return TRUE;
}

BOOL CErksMainDialog::OnCommand(WPARAM wParam, LPARAM lParam)
{
    const UINT id = LOWORD(wParam);

    switch (id)
    {
    case ID_ERKS_FILE_OPEN: OnFileOpen(); return TRUE;
    case ID_ERKS_FILE_SAVE: OnFileSave(); return TRUE;
    case ID_ERKS_FILE_SAVEAS: OnFileSaveAs(); return TRUE;
    case ID_ERKS_FILE_OPEN_EXISTING_DATA: OnFileOpenExistingData(); return TRUE;
    case ID_ERKS_FILE_CREATE_ROAD_AXIS: OnFileCreateRoadAxis(); return TRUE;
    case ID_ERKS_FILE_EXIT: OnFileExit(); return TRUE;
    case ID_ERKS_HELP_ABOUT: OnHelpAbout(); return TRUE;
    default:
        return CDialog::OnCommand(wParam, lParam);
    }
}

static void ShowNotImplemented(LPCTSTR what)
{
    CString msg;
    msg.Format(_T("%s - not implemented yet."), what);
    AfxMessageBox(msg, MB_OK | MB_ICONINFORMATION);
}

void CErksMainDialog::OnFileOpen() { ShowNotImplemented(_T("Open")); }
void CErksMainDialog::OnFileSave() { ShowNotImplemented(_T("Save")); }
void CErksMainDialog::OnFileSaveAs() { ShowNotImplemented(_T("Save As")); }
void CErksMainDialog::OnFileOpenExistingData() { ShowNotImplemented(_T("Open Existing Data")); }
void CErksMainDialog::OnFileCreateRoadAxis() { ShowNotImplemented(_T("Create Road Axis")); }

void CErksMainDialog::OnFileExit()
{
    EndDialog(IDOK);
}

void CErksMainDialog::OnHelpAbout()
{
    AfxMessageBox(_T("Erk-S V1.0\n\nAbout"), MB_OK | MB_ICONINFORMATION);
}
