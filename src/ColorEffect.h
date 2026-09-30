// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <Windows.h>
#include <Magnification.h>
#include <span>
#include <string>

namespace dimmest {
    struct ColorPreset {
        enum Kind { Separator, Neutral, Temperature, Tint };

        Kind kind;
        const wchar_t* id;    /* saved in the settings file */
        const wchar_t* label; /* shown in the tray menu */
        int kelvin;           /* Temperature */
        float r, g, b;        /* Tint: luminance gets painted with this color */
    };

    std::span<const ColorPreset> colorPresets();
    std::wstring colorLabel(const std::wstring& color);

    /* custom colors are stored as "#rrggbb" */
    bool isCustomColor(const std::wstring& color);
    std::wstring customColorId(COLORREF rgb);
    COLORREF customColorValue(const std::wstring& color);

    MAGCOLOREFFECT identityEffect();
    MAGCOLOREFFECT buildColorEffect(int brightness, const std::wstring& color);
}
