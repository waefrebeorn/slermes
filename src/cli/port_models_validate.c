/*
 * port_models_validate.c — faithful port of hermes_cli/models.py:
 * validate_requested_model (plus its helper ensure_lmstudio_model_loaded).
 *
 * validate_requested_model is a ~540-line orchestrator spanning format
 * checks, seven provider families, MoA preset lookup, fuzzy auto-correction,
 * and live /models probing.  All external surface is INJECTED so the logic is
 * testable offline:
 *   - http_fetch_fn        : live /models / lmstudio probes
 *   - moa_presets_fn       : MoA preset names
 *   - catalog_resolver_fn  : provider_model_ids() for codex/oauth/minimax/bedrock
 * No stubs: every branch mirrors the Python control flow.
 */

#include "port_models_validate.h"
#include "port_models_net.h"
#include "port_models_pure.h"
#include "port_models_helpers.h"
#include "model_catalog.h"
#include "model_normalize.h"
#include "libjson/json.h"
#include "libfuzzymatch/fuzzy_match.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <math.h>

static char *xstrdup(const char *s) { return s ? strdup(s) : strdup(""); }

static void strlist_free_local(char **p) { if (!p) return; for (size_t i = 0; p[i]; i++) free(p[i]); free(p); }

/* Character-level similarity ratio (mirrors difflib.SequenceMatcher.ratio):
 * 2*LCS / (len(a)+len(b)), range 0.0..1.0. Used for get_close_matches
 * auto-correction (typo tolerance), independent of fuzzy_ratio's line-based
 * matching which is for find-and-replace. */
static double char_ratio(const char *a, const char *b) {
    if (!a || !b) return 0.0;
    size_t n = strlen(a), m = strlen(b);
    if (n == 0 && m == 0) return 1.0;
    if (n == 0 || m == 0) return 0.0;
    /* DP table for LCS length */
    size_t *prev = calloc(m + 1, sizeof(size_t));
    size_t *cur = calloc(m + 1, sizeof(size_t));
    size_t lcs = 0;
    for (size_t i = 1; i <= n; i++) {
        for (size_t j = 1; j <= m; j++) {
            if (a[i-1] == b[j-1]) cur[j] = prev[j-1] + 1;
            else cur[j] = (prev[j] > cur[j-1]) ? prev[j] : cur[j-1];
        }
        lcs = cur[m];
        size_t *t = prev; prev = cur; cur = t;
    }
    free(prev); free(cur);
    return 2.0 * (double)lcs / (double)(n + m);
}

/* ── fuzzy auto-correct (get_close_matches) ─────────────────────────────── */
/* Returns a malloc'd NULL-terminated array of up to `n` candidate strings
 /* whose char_ratio against `word` is >= cutoff, ordered best-first. Caller
  * frees.  Mirrors difflib.get_close_matches semantics closely enough for the
  * validate use-case (cutoff 0.9 autocorrect / 0.5 suggestions). */
static char **get_close_matches(const char *word, char *const *cands,
                                size_t n, double cutoff) {
    size_t cap = 8, cnt = 0;
    typedef struct { char *s; double r; } hit_t;
    hit_t *hits = calloc(cap, sizeof(hit_t));
    for (size_t i = 0; cands && cands[i]; i++) {
        double r = char_ratio(word, cands[i]);
        if (r >= cutoff) {
            if (cnt + 1 >= cap) { cap *= 2; hits = realloc(hits, cap * sizeof(hit_t)); }
            hits[cnt].s = xstrdup(cands[i]);
            hits[cnt].r = r;
            cnt++;
        }
    }
    /* simple insertion sort by ratio desc */
    for (size_t i = 1; i < cnt; i++) {
        hit_t key = hits[i]; size_t j = i;
        while (j > 0 && hits[j-1].r < key.r) { hits[j] = hits[j-1]; j--; }
        hits[j] = key;
    }
    size_t outn = (n && cnt > n) ? n : cnt;
    char **out = calloc(outn + 1, sizeof(char *));
    for (size_t i = 0; i < outn; i++) out[i] = xstrdup(hits[i].s);
    out[outn] = NULL;
    for (size_t i = 0; i < cnt; i++) free(hits[i].s);
    free(hits);
    if (outn == 0) { free(out); return NULL; }
    return out;
}

/* Build a "  Similar models: `a`, `b`" style suffix from close matches. */
static char *suggestion_text(const char *requested, char *const *cands, size_t n) {
    char **m = get_close_matches(requested, cands, n, 0.5);
    size_t cnt = 0; for (; m && m[cnt]; cnt++);
    if (cnt == 0) { strlist_free_local(m); return xstrdup(""); }
    size_t sz = 64;
    char *out = malloc(sz);
    int len = snprintf(out, sz, "\n  Similar models:");
    for (size_t i = 0; m[i]; i++) {
        char add[512];
        snprintf(add, sizeof(add), " `%s`", m[i]);
        if ((size_t)len + strlen(add) + 1 >= sz) { sz = len + strlen(add) + 64; out = realloc(out, sz); }
        strcat(out, add);
        len += (int)strlen(add);
    }
    strlist_free_local(m);
    return out;
}

/* Parse a JSON array-of-strings (from provider_model_ids / injected resolver)
 * into a malloc'd NULL-terminated array. Returns NULL if not an array. */
static char **parse_id_array(const char *json) {
    if (!json) return NULL;
    json_t *root = json_parse(json, NULL);
    if (!root || root->type != JSON_ARRAY) { if (root) json_free(root); return NULL; }
    char **out = calloc(root->c.count + 1, sizeof(char *));
    size_t o = 0;
    for (size_t i = 0; i < root->c.count; i++) {
        json_t *it = root->c.items[i];
        if (it && it->type == JSON_STRING && it->str_val) out[o++] = xstrdup(it->str_val);
    }
    out[o] = NULL;
    json_free(root);
    return out;
}

/* ── ensure_lmstudio_model_loaded ──────────────────────────────────────── */
/* PoP: ensure_lmstudio_model_loaded @ hermes_cli/models.py:ensure_lmstudio_model_loaded */
/* Injectable HTTP POST to /api/v1/models/load. Returns resolved loaded context
 * length, or -1 on failure. The probe uses the injectable `fetch` transport;
 * the (re)load uses the injectable `post` transport. Either may be NULL when
 * not needed (e.g. already-loaded case needs no POST). */
/* PoP: models_ensure_lmstudio_model_loaded @ hermes_cli/models.py:ensure_lmstudio_model_loaded */
int models_ensure_lmstudio_model_loaded(http_fetch_fn fetch, void *fetch_ctx,
                                         http_post_fn post, void *post_ctx,
                                         const char *model, const char *base_url,
                                         const char *api_key,
                                         int target_context_length, double timeout) {
    (void)timeout;
    char *root = models_lmstudio_server_root(base_url);
    if (!root || !*root) { free(root); return -1; }
    json_t *raw = models_lmstudio_fetch_raw_models(fetch, fetch_ctx, api_key, base_url);
    if (!raw) { free(root); return -1; }

    json_t *models = json_obj_get(raw, "models");
    json_t *target_entry = NULL;
    if (models && models->type == JSON_ARRAY) {
        for (size_t i = 0; i < models->c.count; i++) {
            json_t *m = models->c.items[i];
            if (!m || m->type != JSON_OBJECT) continue;
            json_t *kj = json_obj_get(m, "key");
            json_t *idj = json_obj_get(m, "id");
            const char *k = (kj && kj->type == JSON_STRING) ? kj->str_val : NULL;
            const char *id = (idj && idj->type == JSON_STRING) ? idj->str_val : NULL;
            if ((k && strcmp(k, model ? model : "") == 0) ||
                (id && strcmp(id, model ? model : "") == 0)) { target_entry = m; break; }
        }
    }
    if (!target_entry) { json_free(raw); free(root); return -1; }

    json_t *maxc = json_obj_get(target_entry, "max_context_length");
    if (maxc && maxc->type == JSON_NUMBER && maxc->num_val > 0)
        target_context_length = (int)fmin((double)target_context_length, maxc->num_val);

    /* already-loaded instance with sufficient context? */
    json_t *insts = json_obj_get(target_entry, "loaded_instances");
    if (insts && insts->type == JSON_ARRAY) {
        for (size_t i = 0; i < insts->c.count; i++) {
            json_t *inst = insts->c.items[i];
            if (!inst || inst->type != JSON_OBJECT) continue;
            json_t *cfg = json_obj_get(inst, "config");
            json_t *lc = (cfg && cfg->type == JSON_OBJECT) ? json_obj_get(cfg, "context_length") : NULL;
            if (lc && lc->type == JSON_NUMBER && lc->num_val >= target_context_length) {
                int loaded = (int)lc->num_val;
                json_free(raw); free(root);
                return loaded;
            }
        }
    }

    int result = -1;
    if (post) {
        char url[1100];
        snprintf(url, sizeof(url), "%s/api/v1/models/load", root);
        char hdr[512];
        char *lh = models_lmstudio_request_headers(api_key);
        snprintf(hdr, sizeof(hdr), "%s,\"Content-Type\":\"application/json\"}", lh);
        free(lh);
        char body[256];
        snprintf(body, sizeof(body), "{\"model\":\"%s\",\"context_length\":%d}",
                 model ? model : "", target_context_length);
        if (post(url, hdr, body, post_ctx) == 0) result = target_context_length;
    }
    json_free(raw); free(root);
    return result;
}

/* ── validate_requested_model ──────────────────────────────────────────── */
/* PoP: validate_requested_model @ hermes_cli/models.py:validate_requested_model */
models_validate_result_t models_validate_requested_model(
    http_fetch_fn fetch, void *ctx,
    const char *model_name, const char *provider,
    const char *api_key, const char *base_url, const char *api_mode,
    moa_presets_fn moa_resolve, void *moa_ctx,
    catalog_resolver_fn catalog_resolve, void *cat_ctx) {

    models_validate_result_t r = {0};
    char *requested = xstrdup((model_name && *model_name) ? model_name : "");
    const char *norm_static = model_normalize_provider(provider ? provider : "");
    char *normalized = xstrdup(norm_static);
    if (strcmp(normalized, "openrouter") == 0 && base_url && strstr(base_url, "openrouter.ai") == NULL)
        free(normalized), normalized = xstrdup("custom");

    /* copilot lookup normalization */
    char *requested_for_lookup = xstrdup(requested);
    if (strcmp(normalized, "copilot") == 0) {
        char *n = model_normalize_copilot_model_id(requested);
        if (n && *n) { free(requested_for_lookup); requested_for_lookup = n; }
        else free(n);
    }

    if (!*requested) {
        r.accepted = 0; r.persist = 0; r.recognized = 0;
        r.message = xstrdup("Model name cannot be empty.");
        goto done;
    }

    if (strcmp(normalized, "moa") == 0) {
        char **presets = moa_resolve ? moa_resolve(moa_ctx) : NULL;
        int found = 0;
        for (size_t i = 0; presets && presets[i]; i++)
            if (strcmp(presets[i], requested) == 0) { found = 1; break; }
        if (found) { r.accepted = 1; r.persist = 1; r.recognized = 1; }
        else {
            r.accepted = 0; r.persist = 0; r.recognized = 0;
            char buf[512];
            snprintf(buf, sizeof(buf), "MoA preset `%s` was not found. Run `hermes moa list`.", requested);
            r.message = xstrdup(buf);
        }
        strlist_free_local(presets);
        goto done;
    }

    /* spaces check */
    {
        int sp = 0; for (size_t i = 0; requested[i]; i++) if (isspace((unsigned char)requested[i])) { sp = 1; break; }
        if (sp) {
            r.accepted = 0; r.persist = 0; r.recognized = 0;
            r.message = xstrdup("Model names cannot contain spaces.");
            goto done;
        }
    }

    if (strcmp(normalized, "lmstudio") == 0) {
        char **models = models_probe_lmstudio_models(fetch, ctx, api_key, base_url);
        if (models == NULL) {
            r.accepted = 0; r.persist = 0; r.recognized = 0;
            char buf[512];
            snprintf(buf, sizeof(buf), "Could not reach LM Studio's `/api/v1/models` to validate `%s`.", requested);
            r.message = xstrdup(buf);
        } else if (models[0] == NULL) {
            r.accepted = 0; r.persist = 0; r.recognized = 0;
            char buf[512];
            snprintf(buf, sizeof(buf),
                "LM Studio is reachable but no chat-capable models are loaded. "
                "Load `%s` in LM Studio (Developer tab -> Load Model) and try again.", requested);
            r.message = xstrdup(buf);
        } else {
            int hit = 0;
            for (size_t i = 0; models[i]; i++)
                if (strcmp(models[i], requested_for_lookup) == 0) { hit = 1; break; }
            if (hit) { r.accepted = 1; r.persist = 1; r.recognized = 1; }
            else {
                r.accepted = 0; r.persist = 0; r.recognized = 0;
                char buf[512];
                snprintf(buf, sizeof(buf), "Model `%s` was not found in LM Studio's model listing.", requested);
                r.message = xstrdup(buf);
            }
        }
        strlist_free_local(models);
        goto done;
    }

    if (strcmp(normalized, "custom") == 0 || strncmp(normalized, "custom:", 7) == 0) {
        char *probe = models_probe_api_models(fetch, ctx, api_key, base_url, api_mode);
        char **api_models = NULL;
        if (probe) {
            json_t *p = json_parse(probe, NULL);
            if (p) {
                json_t *m = json_obj_get(p, "models");
                if (m && m->type == JSON_ARRAY) {
                    api_models = calloc(m->c.count + 1, sizeof(char *));
                    size_t o = 0;
                    for (size_t i = 0; i < m->c.count; i++)
                        if (m->c.items[i]->type == JSON_STRING && m->c.items[i]->str_val)
                            api_models[o++] = xstrdup(m->c.items[i]->str_val);
                    api_models[o] = NULL;
                }
                json_t *fb = json_obj_get(p, "used_fallback");
                (void)fb;
                json_free(p);
            }
            free(probe);
        }
        if (api_models != NULL) {
            int hit = 0;
            for (size_t i = 0; api_models[i]; i++)
                if (strcmp(api_models[i], requested_for_lookup) == 0) { hit = 1; break; }
            if (hit) { r.accepted = 1; r.persist = 1; r.recognized = 1; }
            else {
                char **auto_m = get_close_matches(requested_for_lookup, api_models, 1, 0.9);
                if (auto_m && auto_m[0]) {
                    r.accepted = 1; r.persist = 1; r.recognized = 1;
                    char buf[512];
                    snprintf(buf, sizeof(buf), "Auto-corrected `%s` -> `%s`", requested, auto_m[0]);
                    r.corrected_model = xstrdup(auto_m[0]);
                    strlist_free_local(auto_m);
                    r.message = xstrdup(buf);
                } else {
                    char *sug = suggestion_text(requested, api_models, 3);
                    size_t bl = strlen(requested) + strlen(sug) + 256;
                    char *buf = malloc(bl);
                    snprintf(buf, bl,
                        "Note: `%s` was not found in this custom endpoint's model listing. "
                        "It may still work if the server supports hidden or aliased models.%s", requested, sug);
                    r.accepted = 1; r.persist = 1; r.recognized = 0;
                    r.message = buf;
                    free(sug);
                }
            }
            strlist_free_local(api_models);
        } else {
            /* unreachable */
            r.accepted = (api_mode && strcmp(api_mode, "anthropic_messages") == 0) ? 1 : 0;
            r.persist = 1; r.recognized = 0;
            size_t bl = strlen(requested) + 512;
            char *buf = malloc(bl);
            snprintf(buf, bl,
                "Note: could not reach this custom endpoint's model listing. "
                "Hermes will still save `%s`, but the endpoint should expose `/models` for verification.", requested);
            if (api_mode && strcmp(api_mode, "anthropic_messages") == 0) {
                char *nb = realloc(buf, bl + 256);
                strcat(nb, "\n  Many Anthropic-compatible proxies do not implement the Models API "
                           "(GET /v1/models).  The model name has been accepted without verification.");
                buf = nb;
            }
            r.message = buf;
        }
        goto done;
    }

    if (strcmp(normalized, "openai-codex") == 0 || strcmp(normalized, "xai-oauth") == 0) {
        char *cat_json = catalog_resolve ? catalog_resolve(cat_ctx, normalized) : NULL;
        char **catalog_models = parse_id_array(cat_json);
        free(cat_json);
        int cat_resolved = (catalog_models != NULL);
        if (cat_resolved) {
            int hit = 0;
            for (size_t i = 0; catalog_models[i]; i++)
                if (strcmp(catalog_models[i], requested_for_lookup) == 0) { hit = 1; break; }
            if (hit) { r.accepted = 1; r.persist = 1; r.recognized = 1; }
            else {
                char **auto_m = get_close_matches(requested_for_lookup, catalog_models, 1, 0.9);
                if (auto_m && auto_m[0]) {
                    r.accepted = 1; r.persist = 1; r.recognized = 1;
                    char buf[512];
                    snprintf(buf, sizeof(buf), "Auto-corrected `%s` -> `%s`", requested, auto_m[0]);
                    r.corrected_model = xstrdup(auto_m[0]);
                    strlist_free_local(auto_m);
                    r.message = xstrdup(buf);
                } else {
                    const char *prov_label = (strcmp(normalized, "openai-codex") == 0)
                        ? "OpenAI Codex" : "xAI Grok OAuth (SuperGrok / Premium+)";
                    static const char *codex_pref[] = {"gpt-","codex-","o1","o3","o4",NULL};
                    static const char *xai_pref[] = {"grok-",NULL};
                    const char *const *prefs = (strcmp(normalized, "openai-codex") == 0) ? codex_pref : xai_pref;
                    char low[256]; size_t li;
                    for (li = 0; requested_for_lookup[li] && li + 1 < sizeof(low); li++)
                        low[li] = (char)tolower((unsigned char)requested_for_lookup[li]);
                    low[li] = '\0';
                    int plausible = 1;
                    if (prefs[0]) {
                        plausible = 0;
                        for (int p = 0; prefs[p]; p++) if (strncmp(low, prefs[p], strlen(prefs[p])) == 0) { plausible = 1; break; }
                    }
                    char *sug = suggestion_text(requested_for_lookup, catalog_models, 3);
                    if (!plausible) {
                        size_t bl = strlen(requested) + strlen(prov_label) + strlen(sug) + 256;
                        char *buf = malloc(bl);
                        snprintf(buf, bl,
                            "`%s` doesn't look like a %s model and isn't in its listing, so it was not accepted. "
                            "If it belongs to another configured provider, switch with `--provider <slug>` "
                            "(or select it from the `/model` picker).%s", requested, prov_label, sug);
                        r.accepted = 0; r.persist = 0; r.recognized = 0;
                        r.message = buf;
                    } else {
                        size_t bl = strlen(requested) + strlen(prov_label) + strlen(sug) + 256;
                        char *buf = malloc(bl);
                        snprintf(buf, bl,
                            "Note: `%s` was not found in the %s model listing. "
                            "It may still work if your account has access to a newer or hidden model ID.%s",
                            requested, prov_label, sug);
                        r.accepted = 1; r.persist = 1; r.recognized = 0;
                        r.message = buf;
                    }
                    free(sug);
                }
            }
            strlist_free_local(catalog_models);
        } else {
            r.accepted = 1; r.persist = 1; r.recognized = 0;
            r.message = xstrdup("Note: could not resolve the provider catalog to validate the model.");
            strlist_free_local(catalog_models);
        }
        goto done;
    }

    if (strcmp(normalized, "minimax") == 0 || strcmp(normalized, "minimax-cn") == 0) {
        char *cat_json = catalog_resolve ? catalog_resolve(cat_ctx, normalized) : NULL;
        char **catalog_models = parse_id_array(cat_json);
        free(cat_json);
        if (catalog_models != NULL) {
            /* case-insensitive lookup */
            char **lower = calloc(1, sizeof(char *));
            size_t n = 0;
            for (size_t i = 0; catalog_models[i]; i++) {
                char *l = xstrdup(catalog_models[i]);
                for (char *p = l; *p; p++) *p = (char)tolower((unsigned char)*p);
                lower = realloc(lower, (n + 2) * sizeof(char *));
                lower[n++] = l; lower[n] = NULL;
            }
            char rl[256]; size_t ri;
            for (ri = 0; requested_for_lookup[ri] && ri + 1 < sizeof(rl); ri++)
                rl[ri] = (char)tolower((unsigned char)requested_for_lookup[ri]);
            rl[ri] = '\0';
            int hit = 0;
            for (size_t i = 0; lower[i]; i++) if (strcmp(lower[i], rl) == 0) { hit = 1; break; }
            if (hit) { r.accepted = 1; r.persist = 1; r.recognized = 1; }
            else {
                char **auto_m = get_close_matches(rl, lower, 1, 0.9);
                if (auto_m && auto_m[0]) {
                    /* map back to original-case id */
                    const char *orig = NULL;
                    for (size_t i = 0; catalog_models[i]; i++) {
                        char l2[256]; size_t k = 0;
                        for (; catalog_models[i][k] && k + 1 < sizeof(l2); k++)
                            l2[k] = (char)tolower((unsigned char)catalog_models[i][k]);
                        l2[k] = '\0';
                        if (strcmp(l2, auto_m[0]) == 0) { orig = catalog_models[i]; break; }
                    }
                    r.accepted = 1; r.persist = 1; r.recognized = 1;
                    r.corrected_model = xstrdup(orig ? orig : auto_m[0]);
                    strlist_free_local(auto_m);
                    char buf[512];
                    snprintf(buf, sizeof(buf), "Auto-corrected `%s` -> `%s`", requested, r.corrected_model);
                    r.message = xstrdup(buf);
                } else {
                    char *sug = suggestion_text(rl, lower, 3);
                    size_t bl = strlen(requested) + strlen(sug) + 256;
                    char *buf = malloc(bl);
                    snprintf(buf, bl,
                        "Note: `%s` was not found in the MiniMax catalog.%s\n"
                        "  MiniMax does not expose a /models endpoint, so Hermes cannot verify the model name.\n"
                        "  The model may still work if it exists on the server.", requested, sug);
                    r.accepted = 1; r.persist = 1; r.recognized = 0;
                    r.message = buf;
                    free(sug);
                }
            }
            strlist_free_local(lower);
            strlist_free_local(catalog_models);
        } else {
            r.accepted = 1; r.persist = 1; r.recognized = 0;
            r.message = xstrdup("Note: could not resolve the MiniMax catalog to validate the model.");
        }
        goto done;
    }

    if (strcmp(normalized, "anthropic") == 0) {
        char *aj = models_fetch_anthropic_models(fetch, ctx, base_url, api_key);
        char **am = parse_id_array(aj);
        free(aj);
        if (am != NULL) {
            int hit = 0;
            for (size_t i = 0; am[i]; i++) if (strcmp(am[i], requested_for_lookup) == 0) { hit = 1; break; }
            if (hit) { r.accepted = 1; r.persist = 1; r.recognized = 1; }
            else {
                char **auto_m = get_close_matches(requested_for_lookup, am, 1, 0.9);
                if (auto_m && auto_m[0]) {
                    r.accepted = 1; r.persist = 1; r.recognized = 1;
                    char buf[512];
                    snprintf(buf, sizeof(buf), "Auto-corrected `%s` -> `%s`", requested, auto_m[0]);
                    r.corrected_model = xstrdup(auto_m[0]);
                    strlist_free_local(auto_m);
                    r.message = xstrdup(buf);
                } else {
                    char *sug = suggestion_text(requested, am, 3);
                    size_t bl = strlen(requested) + strlen(sug) + 256;
                    char *buf = malloc(bl);
                    snprintf(buf, bl,
                        "Note: `%s` was not found in Anthropic's /v1/models listing. "
                        "It may still work if you have early-access or snapshot IDs.%s", requested, sug);
                    r.accepted = 1; r.persist = 1; r.recognized = 0;
                    r.message = buf;
                    free(sug);
                }
            }
            strlist_free_local(am);
            goto done;
        }
        /* fall through to generic warning path below */
    }

    if (api_mode && strcmp(api_mode, "anthropic_messages") == 0) {
        char **api_models = models_fetch_api_models(fetch, ctx, api_key, base_url, api_mode);
        if (api_models != NULL) {
            int hit = 0;
            for (size_t i = 0; api_models[i]; i++)
                if (strcmp(api_models[i], requested_for_lookup) == 0) { hit = 1; break; }
            if (hit) { r.accepted = 1; r.persist = 1; r.recognized = 1; }
            else {
                char **auto_m = get_close_matches(requested_for_lookup, api_models, 1, 0.9);
                if (auto_m && auto_m[0]) {
                    r.accepted = 1; r.persist = 1; r.recognized = 1;
                    char buf[512];
                    snprintf(buf, sizeof(buf), "Auto-corrected `%s` -> `%s`", requested, auto_m[0]);
                    r.corrected_model = xstrdup(auto_m[0]);
                    strlist_free_local(auto_m);
                    r.message = xstrdup(buf);
                }
            }
            strlist_free_local(api_models);
            goto done;
        }
        r.accepted = 1; r.persist = 1; r.recognized = 0;
        r.message = xstrdup("Note: could not verify the model against this endpoint's model listing. "
                            "Many Anthropic-compatible proxies do not implement GET /v1/models. "
                            "The model name has been accepted without verification.");
        goto done;
    }

    /* generic live probe */
    char **api_models = models_fetch_api_models(fetch, ctx, api_key, base_url, api_mode);
    if (api_models != NULL) {
        char **cmp = api_models;
        if (strcmp(normalized, "gemini") == 0) {
            cmp = calloc(1, sizeof(char *));
            size_t n = 0;
            for (size_t i = 0; api_models[i]; i++) {
                const char *m = api_models[i];
                if (strncmp(m, "models/", 7) == 0) m += 7;
                cmp = realloc(cmp, (n + 2) * sizeof(char *));
                cmp[n++] = xstrdup(m); cmp[n] = NULL;
            }
        }
        int hit = 0;
        for (size_t i = 0; cmp[i]; i++) if (strcmp(cmp[i], requested_for_lookup) == 0) { hit = 1; break; }
        if (hit) { r.accepted = 1; r.persist = 1; r.recognized = 1; }
        else {
            char **auto_m = get_close_matches(requested_for_lookup, cmp, 1, 0.9);
            if (auto_m && auto_m[0]) {
                r.accepted = 1; r.persist = 1; r.recognized = 1;
                r.corrected_model = xstrdup(auto_m[0]);
                char buf[512];
                snprintf(buf, sizeof(buf), "Auto-corrected `%s` -> `%s`", requested, auto_m[0]);
                r.message = xstrdup(buf);
            } else {
                /* curated-catalog fallback (pure, no extra fetch) */
                char key[64], normk[64];
                models_provider_keys(normalized, key, sizeof(key), normk, sizeof(normk));
                char *provs[2] = { normk, NULL };
                int in_cat = models_model_in_provider_catalog(requested_for_lookup, provs);
                if (in_cat) {
                    size_t bl = strlen(requested) + 256;
                    char *buf = malloc(bl);
                    snprintf(buf, bl,
                        "Note: `%s` was not found in the live /v1/models listing but exists in the curated catalog — accepted.",
                        requested);
                    r.accepted = 1; r.persist = 1; r.recognized = 1;
                    r.message = buf;
                } else {
                    char *sug = suggestion_text(requested, cmp, 3);
                    size_t bl = strlen(requested) + strlen(sug) + 256;
                    char *buf = malloc(bl);
                    snprintf(buf, bl, "Model `%s` was not found in this provider's model listing.%s",
                             requested, sug);
                    r.accepted = 0; r.persist = 0; r.recognized = 0;
                    r.message = buf;
                    free(sug);
                }
            }
        }
        strlist_free_local(cmp);
        if (cmp != api_models) strlist_free_local(api_models);
        goto done;
    }

    /* unreachable -> static-catalog fallback, then accept-with-warning */
    char *cat_json = catalog_resolve ? catalog_resolve(cat_ctx, normalized) : NULL;
    char **catalog_models = parse_id_array(cat_json);
    free(cat_json);
    if (catalog_models && catalog_models[0]) {
        char **lower = calloc(1, sizeof(char *));
        size_t n = 0;
        for (size_t i = 0; catalog_models[i]; i++) {
            char *l = xstrdup(catalog_models[i]);
            for (char *p = l; *p; p++) *p = (char)tolower((unsigned char)*p);
            lower = realloc(lower, (n + 2) * sizeof(char *));
            lower[n++] = l; lower[n] = NULL;
        }
        char rl[256]; size_t ri;
        for (ri = 0; requested_for_lookup[ri] && ri + 1 < sizeof(rl); ri++)
            rl[ri] = (char)tolower((unsigned char)requested_for_lookup[ri]);
        rl[ri] = '\0';
        int hit = 0;
        for (size_t i = 0; lower[i]; i++) if (strcmp(lower[i], rl) == 0) { hit = 1; break; }
        if (hit) { r.accepted = 1; r.persist = 1; r.recognized = 1; }
        else {
            char **auto_m = get_close_matches(rl, lower, 1, 0.9);
            if (auto_m && auto_m[0]) {
                const char *orig = NULL;
                for (size_t i = 0; catalog_models[i]; i++) {
                    char l2[256]; size_t k = 0;
                    for (; catalog_models[i][k] && k + 1 < sizeof(l2); k++)
                        l2[k] = (char)tolower((unsigned char)catalog_models[i][k]);
                    l2[k] = '\0';
                    if (strcmp(l2, auto_m[0]) == 0) { orig = catalog_models[i]; break; }
                }
                r.accepted = 1; r.persist = 1; r.recognized = 1;
                r.corrected_model = xstrdup(orig ? orig : auto_m[0]);
                char buf[512];
                snprintf(buf, sizeof(buf), "Auto-corrected `%s` -> `%s`", requested, r.corrected_model);
                r.message = xstrdup(buf);
            } else {
                char *sug = suggestion_text(rl, lower, 3);
                const char *label = normalized; /* _PROVIDER_LABELS simplified to slug */
                size_t bl = strlen(requested) + strlen(label) + strlen(sug) + 256;
                char *buf = malloc(bl);
                snprintf(buf, bl,
                    "Note: `%s` was not found in the %s curated catalog and the /models endpoint was unreachable.%s\n"
                    "  The model may still work if it exists on the provider.", requested, label, sug);
                r.accepted = 1; r.persist = 1; r.recognized = 0;
                r.message = buf;
                free(sug);
            }
        }
        strlist_free_local(lower);
        strlist_free_local(catalog_models);
        goto done;
    }

    /* no catalog at all */
    r.accepted = 1; r.persist = 1; r.recognized = 0;
    size_t bl = strlen(requested) + strlen(normalized) + 256;
    char *buf = malloc(bl);
    snprintf(buf, bl,
        "Note: could not reach the %s API to validate `%s`. If the service isn't down, this model may not be valid.",
        normalized, requested);
    r.message = buf;

done:
    free(requested); free(normalized); free(requested_for_lookup);
    return r;
}

void models_validate_result_free(models_validate_result_t *r) {
    if (!r) return;
    free(r->message); free(r->corrected_model);
    r->message = NULL; r->corrected_model = NULL;
}
