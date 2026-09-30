// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <Windows.h>
#include <Magnification.h>

namespace dimmest {
    /* owns the full-screen color effect. unlike an overlay window there's nothing
    to lose z-order, but Windows can still drop the effect, so it can be re-pushed. */
    class Magnifier {
        public:
            Magnifier();
            ~Magnifier(); /* restores normal colors */
            Magnifier(const Magnifier&) = delete;
            Magnifier& operator=(const Magnifier&) = delete;

            bool initialize();
            void apply(const MAGCOLOREFFECT& effect);
            void reapply();
            void verify(); /* re-push if the active effect isn't ours anymore */

        private:
            bool initialized = false;
            MAGCOLOREFFECT desired;
    };
}
