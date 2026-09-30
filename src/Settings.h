// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <string>

namespace dimmest {
    struct Settings {
        bool enabled = true;
        int brightness = 70;             /* percent of full brightness */
        std::wstring color = L"neutral"; /* a preset id or "#rrggbb", see ColorEffect.h */

        static Settings load();
        void save() const;
    };

    bool isStartWithWindowsEnabled();
    void setStartWithWindowsEnabled(bool enabled);
}
