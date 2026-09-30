// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <Windows.h>
#include <shellapi.h>
#include <string>
#include "Magnifier.h"
#include "Settings.h"

namespace dimmest {
    class TrayApp {
        public:
            explicit TrayApp(HINSTANCE instance);
            ~TrayApp();
            TrayApp(const TrayApp&) = delete;
            TrayApp& operator=(const TrayApp&) = delete;

            bool start();

        private:
            static LRESULT CALLBACK windowProc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam);
            LRESULT handleMessage(HWND window, UINT msg, WPARAM wParam, LPARAM lParam);

            void applySettings();
            void scheduleReapply();
            void showMenu();
            void runMenuCommand(UINT id);
            void pickCustomColor();

            void loadTrayIcon();
            NOTIFYICONDATAW iconData() const;
            std::wstring tooltip() const;
            void addIcon();
            void updateIcon();
            void removeIcon();

            HINSTANCE instance;
            HWND hwnd = nullptr;
            HICON icon = nullptr;
            HPOWERNOTIFY powerNotify = nullptr;
            UINT taskbarCreatedMsg = 0;
            size_t reapplyStage = 0;
            Settings settings;
            Magnifier magnifier;
    };
}
