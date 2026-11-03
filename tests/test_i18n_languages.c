/*
 * test_i18n_languages.c — behavioural test for the C port of
 * agent/i18n_languages.py. Asserts the same invariants the Python module
 * guarantees, so "ported" means "behaves the same", not "compiles".
 */
#include "hermes_json.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

typedef struct { const char *id; const char *endonym; bool rtl; } lang_info_t;

extern const lang_info_t *i18n_bundled_info(const char *lang);
extern size_t i18n_bundled_count(void);
extern const char *i18n_bundled_id_at(size_t i);
extern json_t *i18n_describe_language(const char *lang, bool has_pack,
                                      const char *pack_endonym, bool pack_rtl,
                                      const char *pack_source, bool overlay,
                                      const char *const *surfaces);
extern json_t *i18n_language_options_bundled(const char *const *surfaces);

static int g_pass, g_fail;

static void check(const char *name, int cond)
{
    printf("  %s: %s\n", cond ? "PASS" : "FAIL", name);
    cond ? g_pass++ : g_fail++;
}

static const char *jstr(json_t *o, const char *k)
{
    return json_get_str(o, k, NULL);
}

int main(void)
{
    printf("=== i18n_languages (C) ===\n");

    /* Table */
    check("table has 17 bundled languages", i18n_bundled_count() == 17);
    check("en is English", strcmp(i18n_bundled_info("en")->endonym, "English") == 0);
    check("ar is RTL", i18n_bundled_info("ar")->rtl == true);
    check("de is LTR", i18n_bundled_info("de")->rtl == false);
    check("unknown id returns NULL", i18n_bundled_info("xx") == NULL);

    /* describe_language: bundled precedence */
    json_t *d = i18n_describe_language("ar", false, NULL, false, NULL, false, NULL);
    check("bundled: id", strcmp(jstr(d, "id"), "ar") == 0);
    check("bundled: endonym from table", strcmp(jstr(d, "endonym"), "\xd8\xa7\xd9\x84\xd8\xb9\xd8\xb1\xd8\xa8\xd9\x8a\xd8\xa9") == 0);
    check("bundled: source", strcmp(jstr(d, "source"), "bundled") == 0);
    json_free(d);

    /* describe_language: bundled beats pack metadata */
    d = i18n_describe_language("de", true, "PACKNAME", true, "plugin:foo", false, NULL);
    check("bundled endonym wins over pack", strcmp(jstr(d, "endonym"), "Deutsch") == 0);
    check("bundled rtl wins over pack", json_get_bool(d, "rtl", true) == false);
    check("pack source still reported", strcmp(jstr(d, "source"), "plugin:foo") == 0);
    json_free(d);

    /* describe_language: pack fills in an unbundled language */
    d = i18n_describe_language("xx", true, "Fooish", true, NULL, false, NULL);
    check("pack endonym used", strcmp(jstr(d, "endonym"), "Fooish") == 0);
    check("pack rtl used", json_get_bool(d, "rtl", false) == true);
    check("pack source defaults to 'plugin'", strcmp(jstr(d, "source"), "plugin") == 0);
    json_free(d);

    /* describe_language: unknown, no pack -> bare id / LTR */
    d = i18n_describe_language("zz", false, NULL, false, NULL, false, NULL);
    check("unknown falls back to bare id", strcmp(jstr(d, "endonym"), "zz") == 0);
    check("unknown is LTR", json_get_bool(d, "rtl", true) == false);
    check("unknown is bundled source", strcmp(jstr(d, "source"), "bundled") == 0);
    json_free(d);

    /* describe_language: overlay only applies to unbundled */
    d = i18n_describe_language("yy", false, NULL, false, NULL, true, NULL);
    check("overlay source for unbundled", strcmp(jstr(d, "source"), "overlay") == 0);
    json_free(d);
    d = i18n_describe_language("en", false, NULL, false, NULL, true, NULL);
    check("bundled ignores overlay flag", strcmp(jstr(d, "source"), "bundled") == 0);
    json_free(d);

    /* surfaces */
    const char *surfaces[] = {"agent", "desktop", NULL};
    d = i18n_describe_language("en", false, NULL, false, NULL, false, surfaces);
    json_t *sf = json_obj_get(d, "surfaces");
    check("surfaces copied", sf && json_len(sf) == 2);
    json_free(d);

    /* language_options: en first, then sorted, 17 rows */
    json_t *opts = i18n_language_options_bundled(surfaces);
    check("options is an array of 17", opts && json_len(opts) == 17);
    if (opts && json_len(opts) == 17) {
        json_t *first = json_get(opts, 0);
        check("first row is en", strcmp(jstr(first, "id"), "en") == 0);
        json_t *second = json_get(opts, 1);
        check("second row is af (sorted)", strcmp(jstr(second, "id"), "af") == 0);
        /* rest are sorted ascending */
        int sorted = 1;
        for (size_t i = 1; i + 1 < 17; i++) {
            const char *a = jstr(json_get(opts, i), "id");
            const char *b = jstr(json_get(opts, i + 1), "id");
            if (a && b && strcmp(a, b) > 0) sorted = 0;
        }
        check("rows after en are sorted by id", sorted);
        json_t *last = json_get(opts, 16);
        check("last row is zh-hant", strcmp(jstr(last, "id"), "zh-hant") == 0);
    }
    json_free(opts);

    printf("\nResults: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
