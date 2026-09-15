/* gate: moa_normalize_config — traced mirror of the REAL C port
 * src/hermes_cli/moa_config.c::moa_normalize_config (linked via extra_src).
 * The harness re-walks the port's branch ladder to record rungs, then calls
 * the REAL moa_normalize_config() for the output value.
 *
 * Twin: LIVE hermes_cli/moa_config.py::normalize_moa_config, same rung order.
 *
 * Canonical V event: "default=D|active=A|presets=name:enabled=B,refs=[p/m|..],
 * agg=a/b,mt=N;..." (preset names sorted) — derived from the port's output.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "watercup.h"
#include "json.h"
#include "moa_config.h"

static void strip_ws(char *s) {
    char *pe = s + strlen(s);
    while (pe > s && isspace((unsigned char)pe[-1])) *--pe = '\0';
    char *ps = s;
    while (*ps && isspace((unsigned char)*ps)) ps++;
    if (ps != s) memmove(s, ps, strlen(ps) + 1);
}

static void canon_preset(const json_t *p, char *out, size_t outsz) {
    char refs[2048]; refs[0] = '\0';
    const json_t *rm = p ? json_obj_get(p, "reference_models") : NULL;
    if (rm && rm->type == JSON_ARRAY) {
        for (size_t i = 0; i < rm->c.count; i++) {
            const json_t *s = rm->c.items[i];
            if (!s || s->type != JSON_OBJECT) continue;
            char item[600];
            snprintf(item, sizeof item, "%s%s/%s", i ? "|" : "",
                     json_get_str(s, "provider", ""),
                     json_get_str(s, "model", ""));
            strncat(refs, item, sizeof refs - strlen(refs) - 1);
        }
    }
    const json_t *ag = p ? json_obj_get(p, "aggregator") : NULL;
    const char *ap = ag ? json_get_str(ag, "provider", "") : "";
    const char *am = ag ? json_get_str(ag, "model", "") : "";
    double mt = p ? json_get_num(p, "max_tokens", -1) : -1;
    snprintf(out, outsz, "enabled=%s,refs=[%s],agg=%s/%s,mt=%.17g",
             (p && json_get_bool(p, "enabled", 1)) ? "True" : "False",
             refs, ap ? ap : "", am ? am : "", mt);
}

static int name_cmp(const void *a, const void *b)
{ return strcmp((const char *)a, (const char *)b); }

static void canon_config(const json_t *cfg, char *out, size_t outsz) {
    static char names[32][256];
    size_t n = 0;
    const char *dn = json_get_str(cfg, "default_preset", "");
    const char *an = json_get_str(cfg, "active_preset", "");
    const json_t *ps = json_obj_get(cfg, "presets");
    if (ps && ps->type == JSON_OBJECT)
        for (size_t i = 0; i < ps->c.count && n < 32; i++)
            strncpy(names[n++], ps->c.keys[i], sizeof(names[0]) - 1);
    qsort(names, n, sizeof(names[0]), name_cmp);
    size_t o = (size_t)snprintf(out, outsz, "default=%s|active=%s|presets=", dn, an);
    for (size_t i = 0; i < n; i++) {
        char pb[2304];
        canon_preset(json_obj_get(ps, names[i]), pb, sizeof pb);
        int w = snprintf(out + o, outsz - o, "%s%s:%s", i ? ";" : "", names[i], pb);
        if (w < 0 || (size_t)w >= outsz - o) break;
        o += (size_t)w;
    }
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s normalize_moa_config <raw_json>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("normalize_moa_config");
        json_t *raw_in = json_parse(argv[i], NULL);
        int b_object = raw_in && raw_in->type == JSON_OBJECT;

        /* ---- traced ladder (mirrors moa_normalize_config) ---- */
        WCUP_BRANCH(b_object);                        /* B0: raw is object?   */

        /* cleaned preset-name list, as the port builds `presets` */
        static char kept[32][256];
        int nkept = 0, npres_raw = 0;
        const json_t *presets_raw = NULL;
        int b_presets_obj = 0;
        if (b_object) presets_raw = json_obj_get(raw_in, "presets");
        b_presets_obj = presets_raw && presets_raw->type == JSON_OBJECT;
        WCUP_BRANCH(b_presets_obj);                   /* B1                   */
        if (b_presets_obj) {
            npres_raw = (int)presets_raw->c.count;
            for (size_t k = 0; k < presets_raw->c.count && nkept < 32; k++) {
                WCUP_LOOP_TICK();
                char cn[256] = "";
                if (presets_raw->c.keys[k])
                    strncpy(cn, presets_raw->c.keys[k], sizeof cn - 1);
                strip_ws(cn);
                WCUP_BRANCH(cn[0] != '\0');           /* B2: name kept?       */
                if (cn[0] != '\0') {
                    strncpy(kept[nkept], cn, sizeof(kept[0]) - 1);
                    nkept++;
                }
            }
        }
        int legacy = (nkept == 0);                    /* legacy flat config?  */
        WCUP_BRANCH(legacy);                          /* B3                   */

        /* default_preset validity against the effective preset-name set */
        const char *draw = b_object ? json_get_str(raw_in, "default_preset", "") : "";
        char dn[256]; strncpy(dn, draw ? draw : "", sizeof dn - 1); dn[sizeof dn - 1] = '\0';
        strip_ws(dn);
        int dn_found = 0;
        if (legacy) dn_found = strcmp(dn, "default") == 0;
        else for (int k = 0; k < nkept; k++) if (strcmp(dn, kept[k]) == 0) dn_found = 1;
        WCUP_BRANCH(dn[0] == '\0' || !dn_found);      /* B4: needs fallback?  */
        WCUP_BRANCH(0);                               /* B5: still missing? (post-fallback, never) */

        const char *araw = b_object ? json_get_str(raw_in, "active_preset", "") : "";
        char an[256]; strncpy(an, araw ? araw : "", sizeof an - 1); an[sizeof an - 1] = '\0';
        strip_ws(an);
        int an_found = 0;
        if (legacy) an_found = strcmp(an, "default") == 0;
        else for (int k = 0; k < nkept; k++) if (strcmp(an, kept[k]) == 0) an_found = 1;
        WCUP_BRANCH(!(an[0] != '\0' && an_found));    /* B6: active cleared?  */

        /* ---- real output ---- */
        json_t *res = moa_normalize_config(raw_in);
        char canon[8192];
        canon_config(res, canon, sizeof canon);

        WCUP_VALSTR(canon);
        char cup[32]; WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("normalize_moa_config");
        if (res) json_free(res);
        if (raw_in) json_free(raw_in);
    }
    return 0;
}
