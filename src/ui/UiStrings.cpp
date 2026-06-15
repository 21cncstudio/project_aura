// SPDX-FileCopyrightText: 2025-2026 Volodymyr Papush (21CNCStudio)
// SPDX-License-Identifier: GPL-3.0-or-later
// GPL-3.0-or-later: https://www.gnu.org/licenses/gpl-3.0.html
// Want to use this code in a commercial product while keeping modifications proprietary?
// Purchase a Commercial License: see COMMERCIAL_LICENSE_SUMMARY.md

#include "ui/UiStrings.h"

#include <stddef.h>

namespace UiStrings {

namespace {
constexpr const char *kStringsEn[] = {
#include "ui/strings/UiStrings.en.inc"
};
static_assert(sizeof(kStringsEn) / sizeof(kStringsEn[0]) ==
              static_cast<size_t>(TextId::Count),
              "UiStrings: EN table size mismatch");

constexpr const char *kStringsDe[] = {
#include "ui/strings/UiStrings.de.inc"
};
static_assert(sizeof(kStringsDe) / sizeof(kStringsDe[0]) ==
              static_cast<size_t>(TextId::Count),
              "UiStrings: DE table size mismatch");

constexpr const char *kStringsEs[] = {
#include "ui/strings/UiStrings.es.inc"
};
static_assert(sizeof(kStringsEs) / sizeof(kStringsEs[0]) ==
              static_cast<size_t>(TextId::Count),
              "UiStrings: ES table size mismatch");

constexpr const char *kStringsFr[] = {
#include "ui/strings/UiStrings.fr.inc"
};
static_assert(sizeof(kStringsFr) / sizeof(kStringsFr[0]) ==
              static_cast<size_t>(TextId::Count),
              "UiStrings: FR table size mismatch");

constexpr const char *kStringsIt[] = {
#include "ui/strings/UiStrings.it.inc"
};
static_assert(sizeof(kStringsIt) / sizeof(kStringsIt[0]) ==
              static_cast<size_t>(TextId::Count),
              "UiStrings: IT table size mismatch");

constexpr const char *kStringsPtBr[] = {
#include "ui/strings/UiStrings.ptbr.inc"
};
static_assert(sizeof(kStringsPtBr) / sizeof(kStringsPtBr[0]) ==
              static_cast<size_t>(TextId::Count),
              "UiStrings: PTBR table size mismatch");

constexpr const char *kStringsNl[] = {
#include "ui/strings/UiStrings.nl.inc"
};
static_assert(sizeof(kStringsNl) / sizeof(kStringsNl[0]) ==
              static_cast<size_t>(TextId::Count),
              "UiStrings: NL table size mismatch");

constexpr const char *kStringsZh[] = {
#include "ui/strings/UiStrings.zh.inc"
};
static_assert(sizeof(kStringsZh) / sizeof(kStringsZh[0]) ==
              static_cast<size_t>(TextId::Count),
              "UiStrings: ZH table size mismatch");

constexpr const char *kStringsJa[] = {
#include "ui/strings/UiStrings.ja.inc"
};
static_assert(sizeof(kStringsJa) / sizeof(kStringsJa[0]) ==
              static_cast<size_t>(TextId::Count),
              "UiStrings: JA table size mismatch");

Language g_language = Language::EN;

const char *const *tableFor(Language lang) {
    switch (lang) {
        case Language::DE: return kStringsDe;
        case Language::ES: return kStringsEs;
        case Language::FR: return kStringsFr;
        case Language::IT: return kStringsIt;
        case Language::PT: return kStringsPtBr;
        case Language::NL: return kStringsNl;
        case Language::ZH: return kStringsZh;
        case Language::JA: return kStringsJa;
        case Language::EN:
        default:
            return kStringsEn;
    }
}

} // namespace

void setLanguage(Language lang) {
    g_language = lang;
}

Language language() {
    return g_language;
}

const char *text(TextId id) {
    const size_t index = static_cast<size_t>(id);
    const char *const *table = tableFor(g_language);
    const char *value = table ? table[index] : nullptr;
    if (!value || !*value) {
        value = kStringsEn[index];
    }
    return value ? value : "";
}

} // namespace UiStrings
