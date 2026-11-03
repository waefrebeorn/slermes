/*
 * i18n_languages.c — C port of agent/i18n_languages.py.
 *
 * Language identity for the bundled locales: endonym, script direction, and
 * the picker list. Pure data + pure functions; no IO.
 *
 * Ported faithfully:
 *   describe_language  -> i18n_describe_language
 *   language_options   -> i18n_language_options_bundled
 *
 * Deliberate deviation, called out rather than hidden: the Python
 * language_options() also merges pack registrations and overlay layers
 * (i18n_layers.overlay_languages / pack_info). Those layers are not ported to
 * C, so the C entry point exposes the bundled table only and says so in its
 * name. When the pack/overlay layers land, add i18n_language_options_full().
 */

#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    const char *id;
    const char *endonym;
    bool rtl;
} lang_info_t;

/* Keep sorted by id, as in the Python table. */
static const lang_info_t BUNDLED[] = {
    {"af",      "Afrikaans",  false},
    {"ar",      "العربية",    true},
    {"de",      "Deutsch",    false},
    {"en",      "English",    false},
    {"es",      "Español",    false},
    {"fr",      "Français",   false},
    {"ga",      "Gaeilge",    false},
    {"hu",      "Magyar",     false},
    {"it",      "Italiano",   false},
    {"ja",      "日本語",      false},
    {"ko",      "한국어",      false},
    {"pt",      "Português",  false},
    {"ru",      "Русский",    false},
    {"tr",      "Türkçe",     false},
    {"uk",      "Українська", false},
    {"zh",      "简体中文",    false},
    {"zh-hant", "繁體中文",    false},
};

#define BUNDLED_COUNT (sizeof(BUNDLED) / sizeof(BUNDLED[0]))

static const char *BUNDLED_SOURCE = "bundled";
static const char *OVERLAY_SOURCE = "overlay";

/* PoP: bundled_language_info @ agent/i18n_languages.py:BUNDLED_LANGUAGE_INFO */
const lang_info_t *i18n_bundled_info(const char *lang)
{
    if (!lang) return NULL;
    for (size_t i = 0; i < BUNDLED_COUNT; i++) {
        if (strcmp(BUNDLED[i].id, lang) == 0) return &BUNDLED[i];
    }
    return NULL;
}

size_t i18n_bundled_count(void)
{
    return BUNDLED_COUNT;
}

const char *i18n_bundled_id_at(size_t i)
{
    return i < BUNDLED_COUNT ? BUNDLED[i].id : NULL;
}

/* PoP: describe_language @ agent/i18n_languages.py:describe_language */
/*
 * One picker row. Precedence for endonym/rtl: bundled table -> pack metadata
 * -> bare id / LTR. `source` names the highest layer that supplies the
 * language (plugin:<name> > overlay > bundled).
 *
 * pack_endonym / pack_rtl / pack_source are the pack registration fields; pass
 * has_pack=false (or a NULL pack_endonym and has_pack=false) for no pack.
 * surfaces is a NULL-terminated array of surface names (may be NULL).
 *
 * Returns a malloc'd JSON object; caller frees with json_free().
 */
json_t *i18n_describe_language(const char *lang,
                               bool has_pack,
                               const char *pack_endonym,
                               bool pack_rtl,
                               const char *pack_source,
                               bool overlay,
                               const char *const *surfaces)
{
    if (!lang) return NULL;

    const lang_info_t *b = i18n_bundled_info(lang);

    const char *endonym;
    bool rtl;
    if (b) {
        endonym = b->endonym;
        rtl = b->rtl;
    } else {
        endonym = (has_pack && pack_endonym && pack_endonym[0]) ? pack_endonym : lang;
        rtl = has_pack ? pack_rtl : false;
    }

    const char *source;
    if (has_pack) {
        source = (pack_source && pack_source[0]) ? pack_source : "plugin";
    } else if (overlay && b == NULL) {
        source = OVERLAY_SOURCE;
    } else {
        source = BUNDLED_SOURCE;
    }

    json_t *o = json_object();
    if (!o) return NULL;

    json_set(o, "id", json_string(lang));
    json_set(o, "endonym", json_string(endonym));
    json_set(o, "rtl", json_bool(rtl));
    json_set(o, "source", json_string(source));

    json_t *arr = json_array();
    if (arr) {
        if (surfaces) {
            for (size_t i = 0; surfaces[i]; i++) {
                json_append(arr, json_string(surfaces[i]));
            }
        }
        json_set(o, "surfaces", arr);
    }
    return o;
}

static int cmp_lang_id(const void *a, const void *b)
{
    const lang_info_t *x = *(const lang_info_t *const *)a;
    const lang_info_t *y = *(const lang_info_t *const *)b;
    return strcmp(x->id, y->id);
}

/* PoP: language_options @ agent/i18n_languages.py:language_options */
/*
 * [{id, endonym, rtl, source, surfaces}, ...] for every BUNDLED language,
 * "en" first then sorted by id -- the list every switcher renders.
 *
 * Bundled-only; see the file header for why pack/overlay rows are absent.
 */
json_t *i18n_language_options_bundled(const char *const *surfaces)
{
    const lang_info_t *ordered[BUNDLED_COUNT];
    size_t n = 0;

    /* "en" first, regardless of id order. */
    const lang_info_t *en = i18n_bundled_info("en");
    if (en) ordered[n++] = en;

    const lang_info_t *rest[BUNDLED_COUNT];
    size_t rn = 0;
    for (size_t i = 0; i < BUNDLED_COUNT; i++) {
        if (BUNDLED[i].id == en->id) continue;
        rest[rn++] = &BUNDLED[i];
    }
    qsort(rest, rn, sizeof(rest[0]), cmp_lang_id);
    for (size_t i = 0; i < rn; i++) ordered[n++] = rest[i];

    json_t *out = json_array();
    if (!out) return NULL;
    for (size_t i = 0; i < n; i++) {
        json_t *row = i18n_describe_language(ordered[i]->id, false, NULL, false,
                                             NULL, false, surfaces);
        if (row) json_append(out, row);
    }
    return out;
}
