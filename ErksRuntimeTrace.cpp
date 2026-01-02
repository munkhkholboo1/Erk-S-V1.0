#include "StdAfx.h"
#include "ErksRuntimeTrace.h"

#include <cstdarg>

#pragma comment(lib, "version.lib")

#ifndef _countof
#define _countof(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

namespace ErksRuntimeTrace
{
    void Printf(const wchar_t* fmt, ...)
    {
        wchar_t buf[8192];
        va_list args;
        va_start(args, fmt);
        _vsnwprintf_s(buf, _countof(buf), _TRUNCATE, fmt, args);
        va_end(args);

        acutPrintf(L"\n[ERKS TRACE] %s", buf);
    }

    std::wstring GetModulePath(HMODULE hmod)
    {
        if (!hmod)
            hmod = ::GetModuleHandleW(nullptr);

        auto tryGet = [](HMODULE m, std::wstring& out) -> DWORD {
            out.resize(32768);
            DWORD n = ::GetModuleFileNameW(m, &out[0], (DWORD)out.size());
            if (n == 0)
                return ::GetLastError();
            out.resize(n);
            return ERROR_SUCCESS;
        };

        std::wstring out;
        DWORD err = tryGet(hmod, out);
        if (err == ERROR_SUCCESS)
            return out;

        if (err == ERROR_MOD_NOT_FOUND || err == ERROR_INVALID_HANDLE)
        {
            // Caller passed something that's not a module handle (common when an object pointer is cast to HMODULE).
            // Fall back to the current process module path to avoid noisy logs.
            std::wstring fallback;
            DWORD err2 = tryGet(::GetModuleHandleW(nullptr), fallback);
            if (err2 == ERROR_SUCCESS)
            {
                wchar_t msg[128];
                _snwprintf_s(msg, _countof(msg), _TRUNCATE, L"<invalid module handle %p; using host '%ls'>", hmod, fallback.c_str());
                return msg;
            }
        }

        wchar_t msg[128];
        _snwprintf_s(msg, _countof(msg), _TRUNCATE, L"<GetModuleFileNameW failed err=%lu>", (unsigned long)err);
        return msg;
    }

    bool GetFileVersionString(const std::wstring& filePath, std::wstring& outVersion)
    {
        outVersion.clear();
        if (filePath.empty())
            return false;

        DWORD handle = 0;
        DWORD sz = ::GetFileVersionInfoSizeW(filePath.c_str(), &handle);
        if (sz == 0)
            return false;

        std::vector<BYTE> buf(sz);
        if (!::GetFileVersionInfoW(filePath.c_str(), handle, sz, buf.data()))
            return false;

        VS_FIXEDFILEINFO* ffi = nullptr;
        UINT ffiLen = 0;
        if (!::VerQueryValueW(buf.data(), L"\\", (LPVOID*)&ffi, &ffiLen) || !ffi || ffiLen < sizeof(VS_FIXEDFILEINFO))
            return false;

        const unsigned major = HIWORD(ffi->dwFileVersionMS);
        const unsigned minor = LOWORD(ffi->dwFileVersionMS);
        const unsigned build = HIWORD(ffi->dwFileVersionLS);
        const unsigned rev = LOWORD(ffi->dwFileVersionLS);

        wchar_t v[64];
        _snwprintf_s(v, _countof(v), _TRUNCATE, L"%u.%u.%u.%u", major, minor, build, rev);
        outVersion = v;
        return true;
    }

    void DumpWindowStyles(HWND hwnd, const wchar_t* tag)
    {
        if (!hwnd)
        {
            Printf(L"%s: hwnd=<null>", tag ? tag : L"wnd");
            return;
        }

        const LONG_PTR style = ::GetWindowLongPtr(hwnd, GWL_STYLE);
        const LONG_PTR exStyle = ::GetWindowLongPtr(hwnd, GWL_EXSTYLE);
        const HWND parent = ::GetParent(hwnd);
        const HWND owner = ::GetWindow(hwnd, GW_OWNER);

        Printf(L"%s: hwnd=%p style=0x%p exStyle=0x%p parent=%p owner=%p", tag ? tag : L"wnd", hwnd, (void*)style, (void*)exStyle, parent, owner);
    }

    void DumpModuleIdentity(HMODULE hMod, const wchar_t* tag)
    {
        std::wstring path = GetModulePath(hMod);
        std::wstring ver;
        const bool okVer = GetFileVersionString(path, ver);

        Printf(L"%s: hMod=%p path='%ls' fileVer=%ls", tag ? tag : L"module", hMod, path.c_str(), okVer ? ver.c_str() : L"<n/a>");
    }
}
