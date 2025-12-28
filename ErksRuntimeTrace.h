#pragma once

#include <windows.h>
#include <string>
#include <vector>

// Lightweight runtime trace to AutoCAD command line (acutPrintf).
// Intentionally header-only to avoid project file edits.

namespace ErksRuntimeTrace
{
    void Printf(const wchar_t* fmt, ...);

    std::wstring GetModulePath(HMODULE hmod);

    bool GetFileVersionString(const std::wstring& filePath, std::wstring& outVersion);

    void DumpWindowStyles(HWND hwnd, const wchar_t* tag);

    void DumpModuleIdentity(HMODULE hMod, const wchar_t* tag);
}
