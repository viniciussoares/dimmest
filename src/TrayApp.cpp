// SPDX-License-Identifier: BSD-3-Clause
// Tray icon and menu, plus the Windows events after which the color effect gets pushed again.

#include "TrayApp.h"
#include "ColorEffect.h"
#include "resource.h"
#include "version.h"

#include <CommCtrl.h>
#include <commdlg.h>
#include <WtsApi32.h>
#include <iterator>

using namespace dimmest;

namespace {
    constexpr wchar_t className[] = L"DimmestTrayWindow";
    constexpr UINT WM_TRAYICON = WM_APP + 1;
    constexpr UINT trayIconId = 1;

    constexpr UINT_PTR TIMER_WATCHDOG = 1;
    constexpr UINT_PTR TIMER_REAPPLY = 2;
    constexpr UINT_PTR TIMER_ADD_ICON = 3;

    constexpr UINT watchdogMs = 2000;
    constexpr UINT addIconRetryMs = 2000;

    /* resume/display events can arrive before the display is really back, so push the effect a few times */
    constexpr UINT reapplyDelaysMs[] = { 500, 2000, 5000 };

    constexpr UINT MENU_ID_BRIGHTNESS = 1000; /* + percent */
    constexpr UINT MENU_ID_COLOR = 2000;      /* + preset index */
    constexpr UINT MENU_ID_CUSTOM_COLOR = 2999;
    constexpr UINT MENU_ID_ENABLED = 3000;
    constexpr UINT MENU_ID_STARTUP = 3001;
    constexpr UINT MENU_ID_EXIT = 3002;

    /* GUID_CONSOLE_DISPLAY_STATE, spelled out so we don't need initguid.h */
    constexpr GUID consoleDisplayState =
        { 0x6fe69556, 0x704a, 0x47a0, { 0x8f, 0x24, 0xc2, 0x8d, 0x93, 0x6f, 0xda, 0x47 } };

    UINT checked(bool value) {
        return value ? MF_CHECKED : MF_UNCHECKED;
    }

    /* the taskbar follows the "Windows mode" setting, not the app one */
    bool taskbarUsesLightTheme() {
        DWORD value = 0;
        DWORD size = sizeof(value);
        RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
            L"SystemUsesLightTheme", RRF_RT_REG_DWORD, nullptr, &value, &size);
        return value != 0;
    }

    /* the tray window is hidden at 0,0, so center the color dialog on the monitor under the cursor */
    UINT_PTR CALLBACK centerColorDialog(HWND dialog, UINT msg, WPARAM, LPARAM) {
        if (msg == WM_INITDIALOG) {
            POINT cursor = {};
            GetCursorPos(&cursor);

            MONITORINFO info = {};
            info.cbSize = sizeof(info);
            GetMonitorInfoW(MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST), &info);

            RECT rect = {};
            GetWindowRect(dialog, &rect);

            const RECT& work = info.rcWork;
            const int x = work.left + ((work.right - work.left) - (rect.right - rect.left)) / 2;
            const int y = work.top + ((work.bottom - work.top) - (rect.bottom - rect.top)) / 2;
            SetWindowPos(dialog, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        }
        return 0;
    }
}

TrayApp::TrayApp(HINSTANCE instance)
: instance(instance)
, settings(Settings::load()) {
}

TrayApp::~TrayApp() {
    if (hwnd) {
        DestroyWindow(hwnd);
    }
    if (icon) {
        DestroyIcon(icon);
    }
}

bool TrayApp::start() {
    if (!magnifier.initialize()) {
        MessageBoxW(nullptr,
            L"Couldn't start the Windows Magnification API, so dimmest can't dim the screen.",
            appName, MB_ICONERROR);
        return false;
    }

    WNDCLASSW wc = {};
    wc.lpfnWndProc = &TrayApp::windowProc;
    wc.hInstance = instance;
    wc.lpszClassName = className;
    RegisterClassW(&wc);

    /* a hidden top-level window rather than a message-only one: only top-level
    windows receive broadcasts like TaskbarCreated and WM_DISPLAYCHANGE. */
    CreateWindowExW(WS_EX_TOOLWINDOW, className, appName, WS_POPUP,
        0, 0, 0, 0, nullptr, nullptr, instance, this);

    if (!hwnd) {
        return false;
    }

    taskbarCreatedMsg = RegisterWindowMessageW(L"TaskbarCreated");
    WTSRegisterSessionNotification(hwnd, NOTIFY_FOR_THIS_SESSION);
    powerNotify = RegisterPowerSettingNotification(hwnd, &consoleDisplayState, DEVICE_NOTIFY_WINDOW_HANDLE);
    loadTrayIcon();

    applySettings();
    addIcon();
    SetTimer(hwnd, TIMER_WATCHDOG, watchdogMs, nullptr);
    return true;
}

LRESULT CALLBACK TrayApp::windowProc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_NCCREATE) {
        auto app = static_cast<TrayApp*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        app->hwnd = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    }

    auto app = reinterpret_cast<TrayApp*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (app) {
        return app->handleMessage(window, msg, wParam, lParam);
    }

    return DefWindowProcW(window, msg, wParam, lParam);
}

LRESULT TrayApp::handleMessage(HWND window, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == taskbarCreatedMsg && taskbarCreatedMsg != 0) {
        /* explorer restarted and took our tray icon with it (also sent on some DPI changes, so reload the icon size) */
        loadTrayIcon();
        addIcon();
        scheduleReapply();
        return 0;
    }

    switch (msg) {
        case WM_TRAYICON: {
            switch (LOWORD(lParam)) {
                case NIN_SELECT:
                case NIN_KEYSELECT:
                case WM_CONTEXTMENU:
                    showMenu();
                    break;

                case WM_MBUTTONUP:
                    settings.enabled = !settings.enabled;
                    applySettings();
                    break;
            }
            return 0;
        }

        case WM_TIMER: {
            if (wParam == TIMER_WATCHDOG) {
                magnifier.verify();
            }
            else if (wParam == TIMER_REAPPLY) {
                magnifier.reapply();
                if (++reapplyStage < std::size(reapplyDelaysMs)) {
                    SetTimer(window, TIMER_REAPPLY, reapplyDelaysMs[reapplyStage], nullptr);
                }
                else {
                    KillTimer(window, TIMER_REAPPLY);
                }
            }
            else if (wParam == TIMER_ADD_ICON) {
                addIcon();
            }
            return 0;
        }

        case WM_DISPLAYCHANGE: {
            scheduleReapply();
            return 0;
        }

        case WM_SETTINGCHANGE: {
            /* light/dark theme switched: swap the white and black tray icons */
            if (lParam && wcscmp(reinterpret_cast<const wchar_t*>(lParam), L"ImmersiveColorSet") == 0) {
                loadTrayIcon();
                updateIcon();
            }
            break;
        }

        case WM_POWERBROADCAST: {
            if (wParam == PBT_APMRESUMEAUTOMATIC || wParam == PBT_APMRESUMESUSPEND) {
                scheduleReapply();
            }
            else if (wParam == PBT_POWERSETTINGCHANGE) {
                auto setting = reinterpret_cast<const POWERBROADCAST_SETTING*>(lParam);
                const bool displayOn =
                    setting->PowerSetting == consoleDisplayState &&
                    setting->DataLength == sizeof(DWORD) &&
                    *reinterpret_cast<const DWORD*>(setting->Data) != 0;

                if (displayOn) {
                    scheduleReapply();
                }
            }
            return TRUE;
        }

        case WM_WTSSESSION_CHANGE: {
            if (wParam == WTS_SESSION_UNLOCK || wParam == WTS_CONSOLE_CONNECT || wParam == WTS_REMOTE_CONNECT) {
                scheduleReapply();
            }
            return 0;
        }

        case WM_DESTROY: {
            removeIcon();
            WTSUnRegisterSessionNotification(window);
            if (powerNotify) {
                UnregisterPowerSettingNotification(powerNotify);
                powerNotify = nullptr;
            }
            PostQuitMessage(0);
            return 0;
        }

        case WM_NCDESTROY: {
            SetWindowLongPtrW(window, GWLP_USERDATA, 0);
            hwnd = nullptr;
            break;
        }
    }

    return DefWindowProcW(window, msg, wParam, lParam);
}

void TrayApp::applySettings() {
    magnifier.apply(settings.enabled
        ? buildColorEffect(settings.brightness, settings.color)
        : identityEffect());

    updateIcon();
    settings.save();
}

void TrayApp::scheduleReapply() {
    reapplyStage = 0;
    SetTimer(hwnd, TIMER_REAPPLY, reapplyDelaysMs[0], nullptr);
}

void TrayApp::showMenu() {
    /* "brightness" submenu */
    HMENU brightnessMenu = CreatePopupMenu();
    for (int percent = 100; percent >= 10; percent -= 5) {
        const std::wstring title = std::to_wstring(percent) + (percent == 100 ? L"% (full)" : L"%");
        AppendMenuW(brightnessMenu, MF_STRING | checked(settings.brightness == percent),
            MENU_ID_BRIGHTNESS + percent, title.c_str());
    }

    /* "color" submenu */
    HMENU colorMenu = CreatePopupMenu();
    const auto presets = colorPresets();
    for (size_t i = 0; i < presets.size(); i++) {
        const auto& preset = presets[i];
        if (preset.kind == ColorPreset::Separator) {
            AppendMenuW(colorMenu, MF_SEPARATOR, 0, nullptr);
        }
        else {
            AppendMenuW(colorMenu, MF_STRING | checked(settings.color == preset.id),
                MENU_ID_COLOR + i, preset.label);
        }
    }

    const bool custom = isCustomColor(settings.color);
    const std::wstring customTitle = custom ? L"custom " + settings.color + L"..." : L"custom...";
    AppendMenuW(colorMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(colorMenu, MF_STRING | checked(custom), MENU_ID_CUSTOM_COLOR, customTitle.c_str());

    /* main menu */
    const std::wstring brightnessTitle = L"brightness: " + std::to_wstring(settings.brightness) + L"%";
    const std::wstring colorTitle = L"color: " + colorLabel(settings.color);

    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(brightnessMenu), brightnessTitle.c_str());
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(colorMenu), colorTitle.c_str());
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING | checked(settings.enabled), MENU_ID_ENABLED, L"enabled\tmiddle-click");
    AppendMenuW(menu, MF_STRING | checked(isStartWithWindowsEnabled()), MENU_ID_STARTUP, L"start with windows");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, MENU_ID_EXIT, L"exit");

    /* SetForegroundWindow + WM_NULL: without them the menu won't close when you click elsewhere */
    SetForegroundWindow(hwnd);

    POINT cursor = {};
    GetCursorPos(&cursor);

    const UINT align = GetSystemMetrics(SM_MENUDROPALIGNMENT) ? TPM_RIGHTALIGN : TPM_LEFTALIGN;
    const UINT id = static_cast<UINT>(TrackPopupMenuEx(menu,
        TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | align,
        cursor.x, cursor.y, hwnd, nullptr));

    PostMessageW(hwnd, WM_NULL, 0, 0);
    DestroyMenu(menu); /* also destroys the submenus */

    runMenuCommand(id);
}

void TrayApp::runMenuCommand(UINT id) {
    const auto presets = colorPresets();

    if (id == MENU_ID_EXIT) {
        DestroyWindow(hwnd);
    }
    else if (id == MENU_ID_ENABLED) {
        settings.enabled = !settings.enabled;
        applySettings();
    }
    else if (id == MENU_ID_STARTUP) {
        setStartWithWindowsEnabled(!isStartWithWindowsEnabled());
    }
    else if (id == MENU_ID_CUSTOM_COLOR) {
        pickCustomColor();
    }
    else if (id > MENU_ID_BRIGHTNESS && id <= MENU_ID_BRIGHTNESS + 100) {
        settings.brightness = static_cast<int>(id - MENU_ID_BRIGHTNESS);
        settings.enabled = true; /* picking a level means you want it on */
        applySettings();
    }
    else if (id >= MENU_ID_COLOR && id < MENU_ID_COLOR + presets.size()) {
        settings.color = presets[id - MENU_ID_COLOR].id;
        settings.enabled = true;
        applySettings();
    }
}

void TrayApp::pickCustomColor() {
    static COLORREF palette[16] = {
        RGB(255, 214, 170), RGB(255, 180, 107), RGB(255, 147, 41), RGB(255, 110, 60),
        RGB(255, 80, 80),   RGB(255, 170, 200), RGB(190, 255, 190), RGB(180, 210, 255),
        RGB(255, 255, 255), RGB(255, 255, 255), RGB(255, 255, 255), RGB(255, 255, 255),
        RGB(255, 255, 255), RGB(255, 255, 255), RGB(255, 255, 255), RGB(255, 255, 255),
    };

    CHOOSECOLORW chooser = {};
    chooser.lStructSize = sizeof(chooser);
    chooser.hwndOwner = hwnd;
    chooser.rgbResult = isCustomColor(settings.color) ? customColorValue(settings.color) : palette[0];
    chooser.lpCustColors = palette;
    chooser.Flags = CC_FULLOPEN | CC_RGBINIT | CC_ANYCOLOR | CC_ENABLEHOOK;
    chooser.lpfnHook = centerColorDialog;

    SetForegroundWindow(hwnd);

    if (ChooseColorW(&chooser)) {
        settings.color = customColorId(chooser.rgbResult);
        settings.enabled = true;
        applySettings();
    }
}

void TrayApp::loadTrayIcon() {
    const int id = taskbarUsesLightTheme() ? IDI_TRAY_BLACK : IDI_TRAY_WHITE;

    HICON loaded = nullptr;
    if (SUCCEEDED(LoadIconMetric(instance, MAKEINTRESOURCEW(id), LIM_SMALL, &loaded))) {
        if (icon) {
            DestroyIcon(icon); /* safe: the shell keeps its own copy of whatever we gave it */
        }
        icon = loaded;
    }
}

NOTIFYICONDATAW TrayApp::iconData() const {
    NOTIFYICONDATAW data = {};
    data.cbSize = sizeof(data);
    data.hWnd = hwnd;
    data.uID = trayIconId;
    data.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
    data.uCallbackMessage = WM_TRAYICON;
    data.hIcon = icon;
    wcsncpy_s(data.szTip, tooltip().c_str(), _TRUNCATE);
    return data;
}

std::wstring TrayApp::tooltip() const {
    const std::wstring title = std::wstring(appName) + L" " + appVersion + L"\n";

    if (!settings.enabled) {
        return title + L"off (middle-click to turn on)";
    }

    return title + std::to_wstring(settings.brightness) + L"% brightness, " + colorLabel(settings.color);
}

void TrayApp::addIcon() {
    NOTIFYICONDATAW data = iconData();

    /* NIM_MODIFY covers the case where the icon survived (TaskbarCreated also fires on some DPI changes) */
    if (Shell_NotifyIconW(NIM_ADD, &data) || Shell_NotifyIconW(NIM_MODIFY, &data)) {
        data.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &data);
        KillTimer(hwnd, TIMER_ADD_ICON);
    }
    else {
        /* right after login explorer may not be ready yet, so keep trying */
        SetTimer(hwnd, TIMER_ADD_ICON, addIconRetryMs, nullptr);
    }
}

void TrayApp::updateIcon() {
    NOTIFYICONDATAW data = iconData();
    Shell_NotifyIconW(NIM_MODIFY, &data);
}

void TrayApp::removeIcon() {
    NOTIFYICONDATAW data = iconData();
    Shell_NotifyIconW(NIM_DELETE, &data);
}
