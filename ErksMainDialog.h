#pragma once

#include <afxwin.h>
#include "Resource.h"

class CErksMainDialog : public CDialog
{
public:
    explicit CErksMainDialog(CWnd* pParent = nullptr);
    enum { IDD = IDD_ERKS_MAIN };

protected:
    virtual BOOL OnInitDialog() override;
    virtual BOOL OnCommand(WPARAM wParam, LPARAM lParam) override;

private:
    void OnFileOpen();
    void OnFileSave();
    void OnFileSaveAs();
    void OnFileOpenExistingData();
    void OnFileCreateRoadAxis();
    void OnFileExit();
    void OnHelpAbout();
};
