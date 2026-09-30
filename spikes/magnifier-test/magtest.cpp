// Spike: can the Magnification API dim/tint *everything* (popups, taskbar, Start)?
// Steps through a few color effects; each one waits for OK. Cancel restores and exits.

#include <Windows.h>
#include <Magnification.h>
#include <algorithm>
#include <cmath>
#include <string>

#pragma comment(lib, "magnification.lib")
#pragma comment(lib, "user32.lib")

static MAGCOLOREFFECT identity() {
    MAGCOLOREFFECT e = {};
    for (int i = 0; i < 5; i++) e.transform[i][i] = 1.0f;
    return e;
}

// Row-vector convention: out = [r g b a 1] * M. Scales each channel.
static MAGCOLOREFFECT scale(float r, float g, float b) {
    MAGCOLOREFFECT e = identity();
    e.transform[0][0] = r;
    e.transform[1][1] = g;
    e.transform[2][2] = b;
    return e;
}

// Collapses to luminance, then paints it with the given color (grayscale, red night mode...).
static MAGCOLOREFFECT mono(float r, float g, float b) {
    MAGCOLOREFFECT e = identity();
    const float lum[3] = { 0.299f, 0.587f, 0.114f };
    const float out[3] = { r, g, b };
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            e.transform[i][j] = lum[i] * out[j];
    return e;
}

// Tanner Helland's approximation, same one the original dimmer used.
static void kelvinToRgb(int kelvin, float& r, float& g, float& b) {
    float k = kelvin / 100.0f;
    r = k <= 66 ? 255.0f : 329.698727446f * std::pow(k - 60.0f, -0.1332047592f);
    g = k <= 66 ? 99.4708025861f * std::log(k) - 161.1195681661f
                : 288.1221695283f * std::pow(k - 60.0f, -0.0755148492f);
    b = k >= 66 ? 255.0f : (k <= 19 ? 0.0f : 138.5177312231f * std::log(k - 10.0f) - 305.0447927307f);
    r = std::clamp(r, 0.0f, 255.0f) / 255.0f;
    g = std::clamp(g, 0.0f, 255.0f) / 255.0f;
    b = std::clamp(b, 0.0f, 255.0f) / 255.0f;
}

static MAGCOLOREFFECT warm(int kelvin, float brightness) {
    float r, g, b;
    kelvinToRgb(kelvin, r, g, b);
    return scale(r * brightness, g * brightness, b * brightness);
}

struct Step {
    const wchar_t* name;
    MAGCOLOREFFECT effect;
};

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int) {
    const wchar_t* title = L"Magnifier dim test";

    if (!MagInitialize()) {
        MessageBoxW(nullptr, (L"MagInitialize failed, error " + std::to_wstring(GetLastError())).c_str(),
            title, MB_ICONERROR | MB_TOPMOST);
        return 1;
    }

    const Step steps[] = {
        { L"Dimmed to 50%",                      scale(0.5f, 0.5f, 0.5f) },
        { L"Warm 3400K at 80%",                  warm(3400, 0.8f) },
        { L"Amber tint at 80%",                  mono(1.0f * 0.8f, 0.6f * 0.8f, 0.15f * 0.8f) },
        { L"Red night mode at 70%",              mono(0.7f, 0.0f, 0.0f) },
        { L"Grayscale at 80%",                   mono(0.8f, 0.8f, 0.8f) },
    };
    const int count = (int)(sizeof(steps) / sizeof(steps[0]));

    bool ok = true;
    for (int i = 0; i < count && ok; i++) {
        MAGCOLOREFFECT effect = steps[i].effect;
        if (!MagSetFullscreenColorEffect(&effect)) {
            MessageBoxW(nullptr,
                (std::wstring(L"MagSetFullscreenColorEffect failed on \"") + steps[i].name +
                 L"\", error " + std::to_wstring(GetLastError())).c_str(),
                title, MB_ICONERROR | MB_TOPMOST);
            break;
        }

        std::wstring text =
            L"Step " + std::to_wstring(i + 1) + L" of " + std::to_wstring(count) + L": " + steps[i].name +
            L"\n\nCheck: open Start, right-click the desktop, hover the taskbar.\n"
            L"Are they changed too?\n\nOK = next step, Cancel = stop and restore.";

        ok = MessageBoxW(nullptr, text.c_str(), title, MB_OKCANCEL | MB_TOPMOST | MB_SETFOREGROUND) == IDOK;
    }

    MAGCOLOREFFECT restore = identity();
    MagSetFullscreenColorEffect(&restore);
    MagUninitialize();

    MessageBoxW(nullptr, L"Screen restored. Done.", title, MB_OK | MB_TOPMOST | MB_SETFOREGROUND);
    return 0;
}
