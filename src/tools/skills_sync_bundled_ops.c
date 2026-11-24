/*
 * skills_sync_bundled_ops.c — faithful C11 port of
 * tools/skills_sync_bundled_ops.py:bundled_skill_for_install.
 *
 * PoP: w18_skillssync_176214_bundled_skill_for_install @ tools/skills_sync_bundled_ops.py:bundled_skill_for_install
 *
 * The bundled-skill set (the '_bundled_state()[3]' runtime dependency) is
 * injected as `bundled_names`; the identifier transform itself is pure.
 */
#include "noc_w18_skillssync_176214.h"
#include "hermes_json.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static const char *SS_OWN_PREFIXES[] = {
    "nousresearch/hermes-agent/skills/",
    "skills-sh/nousresearch/hermes-agent/",
};

static int ss_is_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v' ||
           c == 0x1c || c == 0x1d || c == 0x1e || c == 0x1f;
}

/* identifier.strip() then .strip("/"), with NUL written at the new length. */
static char *ss_strip_then_slashes(const char *s) {
    if (!s) return NULL;
    const unsigned char *p = (const unsigned char *)s;
    size_t n = strlen(s);
    size_t a = 0, b = n;
    while (a < b && ss_is_ws(p[a])) ++a;
    while (b > a && ss_is_ws(p[b-1])) --b;
    while (a < b && p[a] == '/') ++a;
    while (b > a && p[b-1] == '/') --b;
    char *out = (char *)malloc(b - a + 1);
    if (out) { memcpy(out, s + a, b - a); out[b - a] = '\0'; }
    return out;
}

static int ss_has_bundle(const json_t *bundled, const char *name) {
    if (!bundled || bundled->type != JSON_ARRAY) return 0;
    for (size_t i = 0; i < json_len(bundled); ++i) {
        json_t *v = json_get(bundled, i);
        if (v && v->type == JSON_STRING && v->str_val && !strcmp(v->str_val, name)) return 1;
    }
    return 0;
}

char *w18_skillssync_176214_bundled_skill_for_install(const char *identifier,
                                                      const json_t *bundled_names) {
    char *ident = ss_strip_then_slashes(identifier);
    if (!ident) return NULL;
    char *lowered = (char *)malloc(strlen(ident) + 1);
    if (!lowered) { free(ident); return NULL; }
    for (size_t i = 0; ident[i]; ++i)
        lowered[i] = (char)tolower((unsigned char)ident[i]);
    lowered[strlen(ident)] = '\0';

    int has_slash = strchr(ident, '/') != NULL;
    int own = 0;
    for (size_t i = 0; i < sizeof(SS_OWN_PREFIXES)/sizeof(SS_OWN_PREFIXES[0]); ++i)
        if (strncmp(lowered, SS_OWN_PREFIXES[i], strlen(SS_OWN_PREFIXES[i])) == 0) { own = 1; break; }

    char *result = NULL;
    if (!(has_slash && !own)) {
        const char *slash = strrchr(ident, '/');
        const char *name = slash ? slash + 1 : ident;
        if (ss_has_bundle(bundled_names, name)) {
            size_t nl = strlen(name);
            result = (char *)malloc(nl + 1);
            if (result) { memcpy(result, name, nl); result[nl] = '\0'; }
        }
    }
    free(lowered);
    free(ident);
    return result;
}
