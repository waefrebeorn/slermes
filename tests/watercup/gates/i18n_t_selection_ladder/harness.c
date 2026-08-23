/* gate: i18n_t_selection_ladder — traced copy of src/agent/i18n.c selection
 * core (resolve_alias / normalize_lang / catalog_lookup / format_string /
 * i18n_t_lang_vfmt) over the same embedded catalog data.
 * LIVE twin: agent/i18n.py::t.
 *
 * Branch ladder (must match twin.py EXACTLY, same rung order):
 *   B1: explicit lang given?
 *   V1: normalized target language
 *   B2: key found in target catalog?
 *   B3: English fallback attempted (target != en)?
 *   B4: still missing -> return bare key
 *   V2: final message
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>
#include "watercup.h"
#include "hermes_i18n.h"
#include "i18n_data.inc"

static const char *wc_resolve_alias(const char *lang) {
    if (!lang || !*lang) return NULL;
    struct { const char *alias; const char *canonical; } aliases[] = {
        {"english", "en"}, {"en-us", "en"}, {"en-gb", "en"},
        {"chinese", "zh"}, {"mandarin", "zh"}, {"zh-cn", "zh"},
        {"zh-hans", "zh"}, {"zh-sg", "zh"},
        {"traditional-chinese", "zh-hant"}, {"traditional_chinese", "zh-hant"},
        {"zh-tw", "zh-hant"}, {"zh-hk", "zh-hant"}, {"zh-mo", "zh-hant"},
        {"japanese", "ja"}, {"jp", "ja"}, {"ja-jp", "ja"},
        {"german", "de"}, {"deutsch", "de"}, {"de-de", "de"},
        {"spanish", "es"}, {"es-es", "es"},
        {"french", "fr"}, {"fr-fr", "fr"},
        {"ukrainian", "uk"}, {"uk-ua", "uk"}, {"ua", "uk"},
        {"turkish", "tr"}, {"tr-tr", "tr"},
        {"korean", "ko"}, {"ko-kr", "ko"},
        {"italian", "it"}, {"it-it", "it"},
        {"portuguese", "pt"}, {"pt-pt", "pt"}, {"pt-br", "pt"},
        {"russian", "ru"}, {"ru-ru", "ru"},
        {"hungarian", "hu"}, {"hu-hu", "hu"},
        {"afrikaans", "af"}, {"af-za", "af"},
        {"irish", "ga"}, {"ga-ie", "ga"},
        {NULL, NULL}
    };
    char lower[64];
    size_t i = 0;
    for (; lang[i] && i < sizeof(lower) - 1; i++)
        lower[i] = (char)tolower((unsigned char)lang[i]);
    lower[i] = '\0';
    for (int a = 0; aliases[a].alias; a++)
        if (strcmp(lower, aliases[a].alias) == 0)
            return aliases[a].canonical;
    const char *dash = strchr(lower, '-');
    if (dash) {
        size_t bl = (size_t)(dash - lower);
        for (int a = 0; aliases[a].alias; a++)
            if (strncmp(lower, aliases[a].alias, bl) == 0 &&
                strlen(aliases[a].alias) == bl)
                return aliases[a].canonical;
    }
    return NULL;
}

static bool wc_is_supported(const char *lang) {
    for (int i = 0; i < _i18n_catalog_count; i++)
        if (strcmp(lang, _i18n_catalogs[i].lang) == 0)
            return true;
    return false;
}

static const char *wc_normalize_lang(const char *input) {
    if (!input || !*input) return "en";
    if (wc_is_supported(input)) return input;
    const char *alias = wc_resolve_alias(input);
    if (alias) return alias;
    return "en";
}

static const i18n_catalog_t *wc_find_catalog(const char *lang) {
    if (!lang) return NULL;
    for (int i = 0; i < _i18n_catalog_count; i++)
        if (strcmp(lang, _i18n_catalogs[i].lang) == 0)
            return &_i18n_catalogs[i];
    return NULL;
}

/* Flatten-style dotted lookup: catalog entries are stored flattened. */
static const char *wc_lookup(const i18n_catalog_t *cat, const char *key) {
    if (!cat || !key) return NULL;
    for (size_t i = 0; i < cat->count; i++) {
        if (cat->entries[i].key[0] == '\0') break;
        if (strcmp(cat->entries[i].key, key) == 0)
            return cat->entries[i].value;
    }
    return NULL;
}

/* {placeholder} substitution (mirrors LIVE str.format(**kwargs) for our use) */
static void wc_format(const char *tpl, char *keys[], char *vals[], size_t nargs,
                      char *out, size_t outsz) {
    size_t pos = 0;
    for (const char *p = tpl; *p && pos < outsz - 1;) {
        if (*p == '{') {
            const char *end = strchr(p + 1, '}');
            if (end) {
                size_t klen = (size_t)(end - p - 1);
                bool found = false;
                for (size_t a = 0; a < nargs; a++) {
                    if (strlen(keys[a]) == klen && strncmp(keys[a], p + 1, klen) == 0) {
                        for (const char *v = vals[a]; *v && pos < outsz - 1; v++)
                            out[pos++] = *v;
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    if (pos + klen + 2 < outsz - 1) {
                        memcpy(out + pos, p, klen + 2);
                        pos += klen + 2;
                    }
                }
                p = end + 1;
                continue;
            }
        }
        out[pos++] = *p++;
    }
    out[pos] = '\0';
}

static void trace_probe(const char *key, const char *lang,
                        char *fk[], char *fv[], size_t nargs) {
    WCUP_BEGIN("i18n_t");
    char result[2048];

    /* B1: explicit lang */
    const char *target;
    if (lang && lang[0]) {
        WCUP_BRANCH(1);
        target = wc_normalize_lang(lang);
    } else {
        WCUP_BRANCH(0);
        const char *env = getenv("HERMES_LANGUAGE");
        target = (env && env[0]) ? wc_normalize_lang(env) : "en";
    }
    WCUP_VALSTR(target);

    const i18n_catalog_t *cat = wc_find_catalog(target);
    const char *value = cat ? wc_lookup(cat, key) : NULL;

    if (!value && strcmp(target, "en") != 0) {          /* B2/B3 */
        WCUP_BRANCH(1);   /* B2: miss -> try fallback */
        cat = wc_find_catalog("en");
        value = cat ? wc_lookup(cat, key) : NULL;
    } else {
        WCUP_BRANCH(0);
    }

    if (!value) {                                        /* B4 */
        WCUP_BRANCH(1);
        snprintf(result, sizeof result, "%s", key);
    } else {
        WCUP_BRANCH(0);
        if (nargs) wc_format(value, fk, fv, nargs, result, sizeof result);
        else snprintf(result, sizeof result, "%s", value);
    }
    WCUP_VALSTR(result);

    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("i18n_t");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s wc_i18n_t <key|lang|kwargs>...\n", argv[0]); return 2; }
    unsetenv("HERMES_LANGUAGE");
    for (int i = 2; i < argc; i++) {
        char probe[2048];
        snprintf(probe, sizeof probe, "%s", argv[i]);
        char *parts[3] = {probe, NULL, NULL};
        int np = 1;
        for (char *p = probe; *p && np < 3; p++)
            if (*p == '|') { *p = '\0'; parts[np++] = p + 1; }
        const char *key = parts[0] ? parts[0] : "";
        const char *lang = parts[1] ? parts[1] : "";
        /* kwargs: k=v,k=v */
        char *fk[16]; char fv[16][256]; size_t nargs = 0;
        if (parts[2] && parts[2][0]) {
            char kv[1024];
            snprintf(kv, sizeof kv, "%s", parts[2]);
            char *save = NULL;
            for (char *tok = strtok_r(kv, ",", &save); tok && nargs < 16;
                 tok = strtok_r(NULL, ",", &save)) {
                char *eq = strchr(tok, '=');
                if (!eq) continue;
                *eq = '\0';
                fk[nargs] = tok;
                snprintf(fv[nargs], sizeof fv[0], "%s", eq + 1);
                nargs++;
            }
        }
        char *fp[16];
        for (size_t a = 0; a < nargs; a++) fp[a] = fk[a];
        char *fvv[16];
        for (size_t a = 0; a < nargs; a++) fvv[a] = fv[a];
        trace_probe(key, lang, fp, fvv, nargs);
    }
    return 0;
}
