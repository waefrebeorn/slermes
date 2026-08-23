/* gate: image_routing_resolve_inference_base_url — traced C copy of
 * src/agent/image_routing.c::image_routing__resolve_inference_base_url.
 * Faithful port of LIVE Python agent/image_routing.py::_resolve_inference_base_url.
 *
 * Input grammar (argv-safe, parsed identically by twin):
 *   provider|cfgspec    ('-' = empty / absent)
 *   cfgspec tokens joined by ';':
 *     mb=<url>                     -> model.base_url
 *     mp=<provider>                -> model.provider
 *     p.<name>.base_url=<url>      -> providers[name].base_url
 *     c.<name>=<url>               -> custom_providers entry {name, base_url}
 * The runtime-override rung is traced as a single branch that is FALSE in this
 * gate (fresh process on both sides carries no live runtime context).
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "watercup.h"

static char *strip_dup(const char *s) {
    while (*s && (unsigned char)*s <= ' ') s++;
    size_t n = strlen(s);
    while (n && (unsigned char)s[n - 1] <= ' ') n--;
    char *out = malloc(n + 1);
    memcpy(out, s, n); out[n] = '\0';
    return out;
}

static char *lower_dup(const char *s) {
    char *out = strip_dup(s);
    for (char *q = out; *q; q++) *q = (char)tolower((unsigned char)*q);
    return out;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s resolve_inference_base_url <spec>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        char spec[2048]; snprintf(spec, sizeof spec, "%s", argv[i]);
        char *bar = strchr(spec, '|');
        if (bar) *bar = '\0';
        const char *provider_in = spec;
        const char *cfgspec = bar ? bar + 1 : "-";
        WCUP_BEGIN("resolve_inference_base_url");
        /* rung 1: runtime main override — empty in a fresh process */
        const char *runtime = "";
        WCUP_BRANCH(runtime[0] != '\0');
        /* parse cfgspec */
        char *model_bu = NULL, *model_prov = NULL;
        char *prov_names[8], *prov_urls[8]; int nprov = 0;
        char *cp_names[8], *cp_urls[8]; int ncp = 0;
        int have_cfg = (strcmp(cfgspec, "-") != 0 && cfgspec[0]);
        if (have_cfg) {
            char *tmp = strdup(cfgspec), *save = tmp;
            while (*save) {
                char *semi = strchr(save, ';');
                if (semi) *semi = '\0';
                char *eq = strchr(save, '=');
                if (eq) {
                    *eq = '\0';
                    const char *key = save, *val = eq + 1;
                    if (strncmp(key, "mb=", 3) == 0 || strcmp(key, "mb") == 0)
                        model_bu = strdup(val);
                    else if (strcmp(key, "mp") == 0)
                        model_prov = strdup(val);
                    else if (strncmp(key, "p.", 2) == 0) {
                        const char *rest = key + 2;
                        const char *dot = strstr(rest, ".base_url");
                        if (dot) {
                            char nm[128];
                            size_t nl = (size_t)(dot - rest);
                            if (nl >= sizeof nm) nl = sizeof nm - 1;
                            memcpy(nm, rest, nl); nm[nl] = '\0';
                            prov_names[nprov] = strdup(nm);
                            prov_urls[nprov] = strdup(val);
                            nprov++;
                        }
                    } else if (strncmp(key, "c.", 2) == 0) {
                        cp_names[ncp] = strdup(key + 2);
                        cp_urls[ncp] = strdup(val);
                        ncp++;
                    }
                }
                if (!semi) break;
                save = semi + 1;
            }
            free(tmp);
        }
        int cfg_is_obj = have_cfg;
        WCUP_BRANCH(!cfg_is_obj);
        if (!cfg_is_obj) {
            WCUP_VALSTR("");
            char cup[32]; WCUP_FINALIZE(cup, sizeof cup);
            printf("%s\n", cup); WCUP_END("resolve_inference_base_url");
            continue;
        }
        /* rung 2: model.base_url */
        char *model_bu_s = model_bu ? strip_dup(model_bu) : strip_dup("");
        int has_mb = (model_bu_s[0] != '\0');
        WCUP_BRANCH(has_mb);
        char result[512]; result[0] = '\0';
        if (has_mb) {
            snprintf(result, sizeof result, "%s", model_bu_s);
            WCUP_VALSTR(result);
            char cup[32]; WCUP_FINALIZE(cup, sizeof cup);
            printf("%s\n", cup); WCUP_END("resolve_inference_base_url");
            continue;
        }
        /* candidate names */
        char *config_provider = model_prov ? strip_dup(model_prov) : strip_dup("");
        const char *inputs[2]; int ninputs = 0;
        char *prov_stripped = strip_dup(provider_in);
        if (prov_stripped[0]) inputs[ninputs++] = prov_stripped;
        if (config_provider[0]) inputs[ninputs++] = config_provider;
        char cands[8][256]; int ncands = 0;
        for (int k = 0; k < ninputs; k++) {
            WCUP_LOOP_TICK();
            const char *p = inputs[k];
            int dup = 0;
            for (int j = 0; j < ncands; j++) if (strcmp(cands[j], p) == 0) { dup = 1; break; }
            WCUP_BRANCH(dup);
            if (!dup && ncands < 8) snprintf(cands[ncands++], 256, "%s", p);
            char *pl = lower_dup(p);
            int is_custom = (strncmp(pl, "custom:", 7) == 0);
            WCUP_BRANCH(is_custom);
            if (is_custom) {
                const char *colon = strchr(p, ':');
                const char *bare = colon ? colon + 1 : p + 7;
                int d2 = 0;
                for (int j = 0; j < ncands; j++) if (strcmp(cands[j], bare) == 0) { d2 = 1; break; }
                WCUP_BRANCH(d2);
                if (!d2 && ncands < 8) snprintf(cands[ncands++], 256, "%s", bare);
            } else {
                char withpfx[256];
                snprintf(withpfx, sizeof withpfx, "custom:%s", p);
                int d2 = 0;
                for (int j = 0; j < ncands; j++) if (strcmp(cands[j], withpfx) == 0) { d2 = 1; break; }
                WCUP_BRANCH(d2);
                if (!d2 && ncands < 8) snprintf(cands[ncands++], 256, "%s", withpfx);
            }
            free(pl);
        }
        /* rung 3: providers[name].base_url */
        int have_providers = (nprov > 0);
        WCUP_BRANCH(have_providers);
        if (have_providers)
        for (int k = 0; k < ncands && !result[0]; k++) {
            WCUP_LOOP_TICK();
            int found = -1;
            for (int j = 0; j < nprov; j++)
                if (strcmp(prov_names[j], cands[k]) == 0) { found = j; break; }
            WCUP_BRANCH(found >= 0);
            if (found >= 0) {
                char *bu = strip_dup(prov_urls[found]);
                WCUP_BRANCH(bu[0] != '\0');
                if (bu[0]) snprintf(result, sizeof result, "%s", bu);
                free(bu);
            }
        }
        /* rung 4: custom_providers */
        int have_custom = (ncp > 0);
        WCUP_BRANCH(have_custom);
        if (have_custom)
        for (int e = 0; e < ncp && !result[0]; e++) {
            WCUP_LOOP_TICK();
            WCUP_BRANCH(0);   /* non-dict entry — impossible in this grammar */
            char *en = strip_dup(cp_names[e]);
            char *el = lower_dup(cp_names[e]);
            int match = 0;
            for (int k = 0; k < ncands; k++) {
                if (strcmp(en, cands[k]) == 0) { match = 1; break; }
                char *cl = lower_dup(cands[k]);
                int hit = (strcmp(el, cl) == 0);
                free(cl);
                if (hit) { match = 1; break; }
            }
            WCUP_BRANCH(match);
            if (match) {
                char *bu = strip_dup(cp_urls[e]);
                WCUP_BRANCH(bu[0] != '\0');
                if (bu[0]) snprintf(result, sizeof result, "%s", bu);
                free(bu);
            }
            free(en); free(el);
        }
        if (!result[0]) WCUP_VALSTR("");
        else WCUP_VALSTR(result);
        char cup[32]; WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup); WCUP_END("resolve_inference_base_url");
    }
    return 0;
}
