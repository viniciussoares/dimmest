// SPDX-License-Identifier: BSD-3-Clause
// Settings live in %APPDATA%\dimmest\dimmest.ini; "start with windows" is the usual HKCU Run key.

#include "Settings.h"
#include "version.h"
#include <Windows.h>
#include <ShlObj.h>
#include <algorithm>

using namespace dimmest;

namespace {
    constexpr wchar_t section[] = L"dimmest";
    constexpr wchar_t runKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";

    std::wstring exePath() {
        std::wstring path(MAX_PATH, L'\0');
        for (;;) {
            const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
            if (length < path.size()) {
                path.resize(length);
                return path;
            }
            path.resize(path.size() * 2);
        }
    }

    std::wstring runCommand() {
        return L"\"" + exePath() + L"\"";
    }

    const std::wstring& configPath() {
        static const std::wstring path = [] {
            std::wstring directory;

            PWSTR appData = nullptr;
            if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &appData))) {
                directory = std::wstring(appData) + L"\\dimmest";
                CreateDirectoryW(directory.c_str(), nullptr);
            }
            CoTaskMemFree(appData);

            if (directory.empty()) { /* fall back to next to the exe */
                directory = exePath();
                directory.resize(directory.find_last_of(L'\\'));
            }

            return directory + L"\\dimmest.ini";
        }();

        return path;
    }
}

namespace dimmest {
    Settings Settings::load() {
        const wchar_t* file = configPath().c_str();
        Settings settings;

        settings.enabled = GetPrivateProfileIntW(section, L"enabled", settings.enabled, file) != 0;

        const int brightness = static_cast<int>(GetPrivateProfileIntW(section, L"brightness", settings.brightness, file));
        settings.brightness = std::clamp(brightness, 10, 100);

        wchar_t color[64] = {};
        GetPrivateProfileStringW(section, L"color", settings.color.c_str(), color, 64, file);
        settings.color = color;

        return settings;
    }

    void Settings::save() const {
        const wchar_t* file = configPath().c_str();
        WritePrivateProfileStringW(section, L"enabled", enabled ? L"1" : L"0", file);
        WritePrivateProfileStringW(section, L"brightness", std::to_wstring(brightness).c_str(), file);
        WritePrivateProfileStringW(section, L"color", color.c_str(), file);
    }

    bool isStartWithWindowsEnabled() {
        wchar_t value[1024] = {};
        DWORD size = sizeof(value);
        if (RegGetValueW(HKEY_CURRENT_USER, runKey, appName, RRF_RT_REG_SZ, nullptr, value, &size) != ERROR_SUCCESS) {
            return false;
        }
        return _wcsicmp(value, runCommand().c_str()) == 0;
    }

    void setStartWithWindowsEnabled(bool enabled) {
        HKEY key;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, runKey, 0, KEY_SET_VALUE, &key) != ERROR_SUCCESS) {
            return;
        }

        if (enabled) {
            const std::wstring command = runCommand();
            RegSetValueExW(key, appName, 0, REG_SZ,
                reinterpret_cast<const BYTE*>(command.c_str()),
                static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
        }
        else {
            RegDeleteValueW(key, appName);
        }

        RegCloseKey(key);
    }
}
