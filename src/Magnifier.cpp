// SPDX-License-Identifier: BSD-3-Clause

#include "Magnifier.h"
#include "ColorEffect.h"
#include <cstring>

using namespace dimmest;

Magnifier::Magnifier()
: desired(identityEffect()) {
}

Magnifier::~Magnifier() {
    if (initialized) {
        MAGCOLOREFFECT normal = identityEffect();
        MagSetFullscreenColorEffect(&normal);
        MagUninitialize();
    }
}

bool Magnifier::initialize() {
    initialized = MagInitialize() != FALSE;
    return initialized;
}

void Magnifier::apply(const MAGCOLOREFFECT& effect) {
    desired = effect;
    reapply();
}

void Magnifier::reapply() {
    if (initialized) {
        MagSetFullscreenColorEffect(&desired);
    }
}

void Magnifier::verify() {
    if (!initialized) {
        return;
    }

    MAGCOLOREFFECT current;
    if (!MagGetFullscreenColorEffect(&current) || std::memcmp(&current, &desired, sizeof(current)) != 0) {
        reapply();
    }
}
