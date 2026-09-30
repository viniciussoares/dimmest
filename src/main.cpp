// SPDX-License-Identifier: BSD-3-Clause

#include <Windows.h>
#include "TrayApp.h"
#include "version.h"

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int) {
    /* two instances would fight over the same full-screen color effect */
    HANDLE mutex = CreateMutexW(nullptr, FALSE, L"Local\\dimmest");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxW(nullptr, L"dimmest is already running. Look for its icon in the system tray.",
            dimmest::appName, MB_ICONINFORMATION);
        return 0;
    }

    int exitCode = 1;
    {
        dimmest::TrayApp app(instance);
        if (app.start()) {
            MSG msg = {};
            while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
            exitCode = static_cast<int>(msg.wParam);
        }
    }

    CloseHandle(mutex);
    return exitCode;
}
