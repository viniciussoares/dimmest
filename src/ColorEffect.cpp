// SPDX-License-Identifier: BSD-3-Clause
// Builds the 5x5 color matrix handed to the Magnification API.
// Row-vector convention: [r g b a 1] * M, so rows are input channels and columns are outputs.

#include "ColorEffect.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cwchar>
#include <cwctype>

using namespace dimmest;

namespace {
    constexpr ColorPreset presets[] = {
        { ColorPreset::Neutral, L"neutral", L"neutral" },
        { ColorPreset::Separator },
        { ColorPreset::Temperature, L"5500k", L"5500K daylight", 5500 },
        { ColorPreset::Temperature, L"4500k", L"4500K", 4500 },
        { ColorPreset::Temperature, L"3400k", L"3400K halogen", 3400 },
        { ColorPreset::Temperature, L"2700k", L"2700K warm bulb", 2700 },
        { ColorPreset::Temperature, L"1900k", L"1900K candle", 1900 },
        { ColorPreset::Separator },
        { ColorPreset::Tint, L"amber", L"amber", 0, 1.0f, 0.6f, 0.15f },
        { ColorPreset::Tint, L"red", L"red (night vision)", 0, 1.0f, 0.0f, 0.0f },
        { ColorPreset::Tint, L"sepia", L"sepia", 0, 1.0f, 0.82f, 0.6f },
        { ColorPreset::Tint, L"gray", L"grayscale", 0, 1.0f, 1.0f, 1.0f },
    };

    struct Rgb {
        float r, g, b;
    };

    Rgb times(Rgb c, float f) {
        return { c.r * f, c.g * f, c.b * f };
    }

    /* multiplies each channel, like looking through colored glass */
    MAGCOLOREFFECT scale(Rgb c) {
        MAGCOLOREFFECT e = identityEffect();
        e.transform[0][0] = c.r;
        e.transform[1][1] = c.g;
        e.transform[2][2] = c.b;
        return e;
    }

    /* collapses to luminance, then paints it with a single color */
    MAGCOLOREFFECT tint(Rgb c) {
        const float luma[3] = { 0.299f, 0.587f, 0.114f };
        const float out[3] = { c.r, c.g, c.b };
        MAGCOLOREFFECT e = identityEffect();
        for (int in = 0; in < 3; in++) {
            for (int o = 0; o < 3; o++) {
                e.transform[in][o] = luma[in] * out[o];
            }
        }
        return e;
    }

    /* Tanner Helland's blackbody approximation, as used by the original dimmer */
    Rgb kelvinToRgb(int kelvin) {
        const float k = kelvin / 100.0f;

        float r = k <= 66 ? 255.0f : 329.698727446f * std::pow(k - 60.0f, -0.1332047592f);

        float g = k <= 66
            ? 99.4708025861f * std::log(k) - 161.1195681661f
            : 288.1221695283f * std::pow(k - 60.0f, -0.0755148492f);

        float b = k >= 66 ? 255.0f
            : k <= 19 ? 0.0f
            : 138.5177312231f * std::log(k - 10.0f) - 305.0447927307f;

        return {
            std::clamp(r, 0.0f, 255.0f) / 255.0f,
            std::clamp(g, 0.0f, 255.0f) / 255.0f,
            std::clamp(b, 0.0f, 255.0f) / 255.0f
        };
    }
}

namespace dimmest {
    std::span<const ColorPreset> colorPresets() {
        return presets;
    }

    std::wstring colorLabel(const std::wstring& color) {
        for (const auto& preset : presets) {
            if (preset.id && color == preset.id) {
                return preset.label;
            }
        }
        return isCustomColor(color) ? L"custom " + color : L"neutral";
    }

    bool isCustomColor(const std::wstring& color) {
        return color.size() == 7 && color[0] == L'#' &&
            std::all_of(color.begin() + 1, color.end(), [](wchar_t c) { return std::iswxdigit(c) != 0; });
    }

    std::wstring customColorId(COLORREF rgb) {
        wchar_t id[8];
        swprintf_s(id, L"#%02x%02x%02x", GetRValue(rgb), GetGValue(rgb), GetBValue(rgb));
        return id;
    }

    COLORREF customColorValue(const std::wstring& color) {
        if (!isCustomColor(color)) {
            return RGB(255, 255, 255);
        }
        const unsigned long v = std::wcstoul(color.c_str() + 1, nullptr, 16);
        return RGB((v >> 16) & 0xff, (v >> 8) & 0xff, v & 0xff);
    }

    MAGCOLOREFFECT identityEffect() {
        MAGCOLOREFFECT e = {};
        for (int i = 0; i < 5; i++) {
            e.transform[i][i] = 1.0f;
        }
        return e;
    }

    MAGCOLOREFFECT buildColorEffect(int brightness, const std::wstring& color) {
        const float level = std::clamp(brightness, 0, 100) / 100.0f;

        if (isCustomColor(color)) {
            const COLORREF c = customColorValue(color);
            const Rgb rgb = { GetRValue(c) / 255.0f, GetGValue(c) / 255.0f, GetBValue(c) / 255.0f };

            /* the custom color only picks the hue; how dark it gets is the brightness setting's job.
            this also means picking black can't black out the screen. */
            const float peak = std::max({ rgb.r, rgb.g, rgb.b });
            if (peak > 0.0f) {
                return scale(times(rgb, level / peak));
            }
        }

        for (const auto& preset : presets) {
            if (!preset.id || color != preset.id) {
                continue;
            }
            if (preset.kind == ColorPreset::Temperature) {
                return scale(times(kelvinToRgb(preset.kelvin), level));
            }
            if (preset.kind == ColorPreset::Tint) {
                return tint(times({ preset.r, preset.g, preset.b }, level));
            }
            break;
        }

        return scale({ level, level, level });
    }
}
