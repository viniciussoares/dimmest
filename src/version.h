// SPDX-License-Identifier: BSD-3-Clause
// Single source of truth for the version; also included by dimmest.rc.

#ifndef DIMMEST_VERSION_H
#define DIMMEST_VERSION_H

#define DIMMEST_VERSION_MAJOR 0
#define DIMMEST_VERSION_MINOR 1
#define DIMMEST_VERSION_PATCH 0
#define DIMMEST_VERSION "0.1.0"

#ifndef RC_INVOKED
namespace dimmest {
    constexpr wchar_t appName[] = L"dimmest";
    constexpr wchar_t appVersion[] = L"v" DIMMEST_VERSION;
}
#endif

#endif
