/* gate: auth_is_provider_explicitly_configured — traced copy of
 * src/hermes_cli/auth_store.c::is_provider_explicitly_configured.
 * LIVE twin: hermes_cli/auth.py::is_provider_explicitly_configured.
 *
 * The C ladder is copied rung-for-rung from the port; state that the port
 * reads from disk/env (auth.json active_provider + credential_pool,
 * config.yaml model/moa tree, provider env vars) arrives via the input
 * encoding below so the traced path itself stays pure. The provider
 * registry is a verbatim mini-copy of the two entries exercised
 * ("openai-api" api_key/{OPENAI_API_KEY}, "nous" oauth_device_code) —
 * see gate.json notes.
 *
 * Branch ladder (must match twin.py EXACTLY, same rung order):
 *   B0: normalized provider empty -> false
 *   R1: auth.json active_provider matches          -> true
 *   R2a: config model.provider matches             -> true
 *   R2b: moa.reference_models[i].provider matches  -> true (per-slot B)
 *   R2c: moa.aggregator.provider matches           -> true
 *   R2d: presets[p].reference_models/aggregator    -> true (per-slot B)
 *   R3: registry api_key provider + env var usable -> true (per-var B)
 *   R4: pool entry source env:<var> usable / manual-family -> true
 *   else                                           -> false
 *
 * Input encoding: "<provider|active|cfg_json|envspec|pool_json>"
 * ('-' = absent; cfg/pool are JSON and never contain '|';
 *  envspec = VAR=value or '-').
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include "watercup.h"
#include "hermes_json.h"

#define EMIT_BOOL(out) do { \
    WCUP_VALSTR(out); \
    char cup_[32]; \
    WCUP_FINALIZE(cup_, sizeof cup_); \
    printf("%s\n", cup_); \
    WCUP_END(out); \
} while (0)

static void lc_norm(const char *src, char *dst, size_t sz) {
    size_t i = 0;
    if (src) {
        while (*src && isspace((unsigned char)*src)) src++;
        size_t n = strlen(src);
        while (n > 0 && isspace((unsigned char)src[n - 1])) n--;
        for (; i < n && i < sz - 1; i++)
            dst[i] = (char)tolower((unsigned char)src[i]);
    }
    dst[i] = '\0';
}

/* verbatim semantics of has_usable_secret (port_auth_na.c / auth.py) */
static int usable_secret(const char *value) {
    static const char *PLACEHOLDER[] = {
        "*", "**", "***", "changeme", "your_api_key", "your_api_key_here",
        "your-api-key", "placeholder", "example", "dummy", "null", "none", NULL
    };
    if (!value) return 0;
    const char *p = value;
    while (*p == ' ' || *p == '\t') p++;
    size_t L = strlen(p);
    while (L > 0 && (p[L - 1] == ' ' || p[L - 1] == '\t')) L--;
    if (L < 4) return 0;
    char buf[256];
    if (L >= sizeof(buf)) L = sizeof(buf) - 1;
    memcpy(buf, p, L); buf[L] = '\0';
    for (int i = 0; PLACEHOLDER[i]; i++)
        if (strcasecmp(buf, PLACEHOLDER[i]) == 0) return 0;
    return 1;
}

/* mini provider registry — verbatim rows from src/hermes_cli/provider_registry.c */
static const char *registry_auth_type(const char *id) {
    if (strcmp(id, "openai-api") == 0) return "api_key";
    if (strcmp(id, "nous") == 0) return "oauth_device_code";
    return NULL;
}
static const char *const *registry_env_vars(const char *id) {
    static const char *const OPENAI[] = { "OPENAI_API_KEY", NULL };
    if (strcmp(id, "openai-api") == 0) return OPENAI;
    return NULL;
}

static int moa_slot_matches(const json_t *slot, const char *norm) {
    if (!slot || slot->type != JSON_OBJECT) return 0;
    char p[128];
    lc_norm(json_get_str((json_t *)slot, "provider", ""), p, sizeof p);
    return p[0] && strcmp(p, norm) == 0;
}

static void is_provider_explicitly_configured(const char *provider_id,
                                              const char *active_raw,
                                              const char *cfg_json,
                                              const char *envspec,
                                              const char *pool_json) {
    WCUP_BEGIN("is_provider_explicitly_configured");
    char norm[128];
    lc_norm(provider_id, norm, sizeof norm);
    if (!norm[0]) {                                   /* B0 */
        WCUP_BRANCH(1);
        EMIT_BOOL("false");
        return;
    }
    WCUP_BRANCH(0);

    /* 1. auth.json active_provider */
    {
        char act[128];
        lc_norm(active_raw, act, sizeof act);
        if (act[0] && strcmp(act, norm) == 0) {       /* R1 */
            WCUP_BRANCH(1);
            EMIT_BOOL("true");
            return;
        }
        WCUP_BRANCH(0);
    }

    /* 2. config.yaml model.provider + MoA slots */
    json_t *cfg = cfg_json[0] ? json_parse(cfg_json, NULL) : NULL;
    if (cfg) {
        json_t *model = json_obj_get(cfg, "model");
        if (model && model->type == JSON_OBJECT) {
            char cp[128];
            lc_norm(json_get_str(model, "provider", ""), cp, sizeof cp);
            if (cp[0] && strcmp(cp, norm) == 0) {     /* R2a */
                WCUP_BRANCH(1);
                json_free(cfg);
                EMIT_BOOL("true");
                return;
            }
        }
        WCUP_BRANCH(0);
        json_t *moa = json_obj_get(cfg, "moa");
        if (moa && moa->type == JSON_OBJECT) {
            json_t *refs = json_obj_get(moa, "reference_models");
            if (refs && refs->type == JSON_ARRAY) {
                for (size_t i = 0; i < refs->c.count; i++) {
                    if (moa_slot_matches(refs->c.items[i], norm)) {  /* R2b */
                        WCUP_BRANCH(1);
                        json_free(cfg);
                        EMIT_BOOL("true");
                        return;
                    }
                    WCUP_BRANCH(0);
                }
            }
            if (moa_slot_matches(json_obj_get(moa, "aggregator"), norm)) {  /* R2c */
                WCUP_BRANCH(1);
                json_free(cfg);
                EMIT_BOOL("true");
                return;
            }
            WCUP_BRANCH(0);
            json_t *presets = json_obj_get(moa, "presets");
            if (presets && presets->type == JSON_OBJECT) {
                for (size_t k = 0; k < presets->c.count; k++) {
                    json_t *preset = presets->c.items[k];
                    if (!preset || preset->type != JSON_OBJECT) continue;
                    json_t *prefs = json_obj_get(preset, "reference_models");
                    if (prefs && prefs->type == JSON_ARRAY) {
                        for (size_t i = 0; i < prefs->c.count; i++) {
                            if (moa_slot_matches(prefs->c.items[i], norm)) {  /* R2d */
                                WCUP_BRANCH(1);
                                json_free(cfg);
                                EMIT_BOOL("true");
                                return;
                            }
                            WCUP_BRANCH(0);
                        }
                    }
                    if (moa_slot_matches(json_obj_get(preset, "aggregator"), norm)) {
                        WCUP_BRANCH(1);
                        json_free(cfg);
                        EMIT_BOOL("true");
                        return;
                    }
                    WCUP_BRANCH(0);
                }
            }
        }
        json_free(cfg);
    }

    /* 3. provider-specific env vars (excluding CLAUDE_CODE_OAUTH_TOKEN) */
    const char *at = registry_auth_type(norm);
    if (at && strcmp(at, "api_key") == 0) {
        WCUP_BRANCH(1);
        const char *const *vars = registry_env_vars(norm);
        if (vars) {
            for (int i = 0; vars[i]; i++) {
                const char *var = vars[i];
                if (strcmp(var, "CLAUDE_CODE_OAUTH_TOKEN") == 0) continue;
                const char *val = getenv(var);
                if (val && usable_secret(val)) {      /* R3 */
                    WCUP_BRANCH(1);
                    EMIT_BOOL("true");
                    return;
                }
                WCUP_BRANCH(0);
            }
        }
    } else {
        WCUP_BRANCH(0);
    }

    /* 4. explicit-flow credential-pool entries */
    json_t *entries = pool_json[0] ? json_parse(pool_json, NULL) : NULL;
    if (entries && entries->type != JSON_ARRAY) { json_free(entries); entries = NULL; }
    if (entries) {
        for (size_t i = 0; i < entries->c.count; i++) {
            json_t *e = entries->c.items[i];
            if (!e || e->type != JSON_OBJECT) continue;
            char src[256];
            lc_norm(json_get_str(e, "source", ""), src, sizeof src);
            if (!src[0]) continue;
            if (strncmp(src, "env:", 4) == 0) {
                WCUP_BRANCH(1);
                const char *orig = json_get_str(e, "source", "");
                const char *colon = strchr(orig, ':');
                if (colon && colon[1]) {
                    char vn[128];
                    const char *s = colon + 1;
                    while (*s && isspace((unsigned char)*s)) s++;
                    size_t n = strlen(s);
                    while (n > 0 && isspace((unsigned char)s[n - 1])) n--;
                    if (n >= sizeof(vn)) n = sizeof(vn) - 1;
                    memcpy(vn, s, n); vn[n] = '\0';
                    const char *val = vn[0] ? getenv(vn) : NULL;
                    if (val && usable_secret(val)) {  /* R4 env-backed */
                        WCUP_BRANCH(1);
                        json_free(entries);
                        EMIT_BOOL("true");
                        return;
                    }
                    WCUP_BRANCH(0);
                }
                continue;
            }
            WCUP_BRANCH(0);
            if (strcmp(src, "device_code") == 0 || strcmp(src, "loopback_pkce") == 0 ||
                strcmp(src, "hermes_pkce") == 0 || strcmp(src, "manual") == 0 ||
                strncmp(src, "manual:", 7) == 0) {    /* R4 explicit flow */
                WCUP_BRANCH(1);
                json_free(entries);
                EMIT_BOOL("true");
                return;
            }
            WCUP_BRANCH(0);
        }
        json_free(entries);
    }
    EMIT_BOOL("false");
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s is_provider_explicitly_configured "
                "<provider|active|cfg_json|envspec|pool_json>...\n", argv[0]);
        return 2;
    }
    for (int k = 2; k < argc; k++) {
        char *tok = strdup(argv[k]);
        /* fields: provider | active | cfg_json | envspec | pool_json */
        char *f[4];
        int nf = 0;
        for (char *p = tok; *p && nf < 4; p++) {
            if (*p == '|') { *p = '\0'; f[nf++] = p + 1; }
        }
        if (nf < 4) { fprintf(stderr, "bad input: %s\n", argv[k]); return 2; }
        const char *provider = tok;
        const char *active = strcmp(f[0], "-") == 0 ? "" : f[0];
        const char *cfg = strcmp(f[1], "-") == 0 ? "" : f[1];
        const char *envspec = f[2];
        const char *pool = strcmp(f[3], "-") == 0 ? "" : f[3];

        /* apply env spec: VAR=value or clear VAR ('-') */
        if (strcmp(envspec, "-") == 0) {
            unsetenv("OPENAI_API_KEY");
        } else {
            char eq = 0;
            char *eqp = strchr(envspec, '=');
            if (eqp) { eq = *eqp; *eqp = '\0'; }
            setenv(envspec, eqp ? eqp + 1 : "", 1);
            if (eqp) *eqp = (char)eq;
        }
        is_provider_explicitly_configured(provider, active, cfg, envspec, pool);
        free(tok);
    }
    return 0;
}
