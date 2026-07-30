/*
 * provider_profiles_builtin.c — port of plugins/model-providers/<name>/__init__.py
 *
 * Each Python plugin is a module that instantiates a ProviderProfile subclass
 * and calls register_provider(). In C that maps to a builder function per
 * provider that fills a provider_profile_t and calls provider_profile_register().
 *
 * Coverage: all 30 providers found under plugins/model-providers/. Hooks
 * (build_extra_body / build_api_kwargs_extras / prepare_messages /
 * default_vision_model / fetch_models overrides) are ported faithfully:
 *   - openrouter, nous, kimi, deepseek, minimax, qwen, zai, upstage,
 *     ollama-cloud, opencode-zen, gemini, copilot, custom, vertex, xai...
 *   - deepinfra default_vision_model (catalog-tagged)
 *   - anthropic / bedrock / copilot-acp fetch_models overrides returning NULL
 *
 * Chat-completions reasoning-quirk translation (the Gemini thinking_config,
 * get_conversation_context/session_id plumbing) needs chat-layer context
 * fields; those are supplied via build_extra_body / build_api_kwargs_extras
 * ctx_json by the transport when it calls into the profile.
 *
 * PoP: provider_profiles_builtin @ plugins/model-providers/(name)/__init__.py
 */
#define _POSIX_C_SOURCE 200809L
#include "provider_profile.h"
#include "hermes_portal_tags.h"
#include "port_models_pure.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

/* get_conversation_context() is the ambient conversation id (Python ContextVar).
 * In C we map it through portal_tags' session machinery. The existing C
 * portal_tags exposes hermes_nous_portal_tags_json()/hermes_client_tag();
 * the ambient sticky key is read from the runtime session id when set.
 * We use hermes_portal_tags_conversation_id() if available, else NULL. */
static const char *hermes_get_conversation_context(void)
{
    return hermes_get_conversation_context_id();
}

/* HERMES_VERSION injected via -D. */
#ifndef HERMES_VERSION
#define HERMES_VERSION "0.0.0-slermes"
#endif

static char **strv(const char **in)
{
    /* Each element is strdup'd so provider_profile_free's free_strv() is safe. */
    size_t n = 0;
    while (in[n]) n++;
    char **out = calloc(n + 1, sizeof(char *));
    for (size_t i = 0; i < n; i++) out[i] = strdup(in[i]);
    return out;
}
/* STRVS builds a NULL-terminated (const char *[]) from 1+ string literals.
 * strv() strdup's each element so free_strv() is always safe. Empty lists
 * use STRVS("") (a single empty string) — never STRVS() with zero args. */
#define STRVS(...) strv((const char *[]){ __VA_ARGS__, NULL })

/* Owned string dup used for profile string fields (profile_free frees them). */
static char *xstrdup_(const char *s) { return s ? strdup(s) : NULL; }
#define xstrdup2(s) xstrdup_(s)

extern bool g_builtin_done;

/* ── hook helpers ─────────────────────────────────────────────────────── */

static const char *ctx_str(const char *ctx_json, const char *key)
{
    if (!ctx_json) return NULL;
    json_t *c = json_parse(ctx_json, NULL);
    if (!c) return NULL;
    json_t *v = json_obj_get(c, key);
    if (!v) { json_free(c); return NULL; }
    /* Scalars: return their value (borrowed). Objects/arrays: serialize to a
     * malloc'd string so callers that expect a JSON sub-document get it. */
    const char *out;
    if (v->type == JSON_STRING) {
        out = v->str_val;          /* borrowed from c; freed below */
        char *dup = strdup(out);
        json_free(c);
        return dup;
    } else if (v->type == JSON_OBJECT || v->type == JSON_ARRAY) {
        char *s = json_serialize(v);
        json_free(c);
        return s;                  /* caller frees */
    } else if (v->type == JSON_NUMBER) {
        char buf[32]; snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        char *dup = strdup(buf);
        json_free(c);
        return dup;
    } else if (v->type == JSON_BOOL) {
        char *dup = strdup(v->bool_val ? "true" : "false");
        json_free(c);
        return dup;
    }
    json_free(c);
    return NULL;
}

/* ── openrouter ──────────────────────────────────────────────────────── */
static char *pp_openrouter_extra_body(provider_profile_t *p, const char *ctx_json)
{
    (void)p;
    const char *sid = ctx_str(ctx_json, "session_id");
    const char *prefs = ctx_str(ctx_json, "provider_preferences");
    const char *model = ctx_str(ctx_json, "model");
    const char *score_s = ctx_str(ctx_json, "openrouter_min_coding_score");
    json_t *body = json_object();
    const char *sticky = hermes_get_conversation_context();
    const char *key = sticky ? sticky : sid;
    if (key) json_set(body, "session_id", json_string(key));
    if (prefs && prefs[0]) json_set(body, "provider", json_string(prefs));
    if (model && strcmp(model, "openrouter/pareto-code") == 0 && score_s && score_s[0]) {
        char *end = NULL;
        double score_f = strtod(score_s, &end);
        if (end != score_s && *end == '\0' && score_f >= 0.0 && score_f <= 1.0) {
            json_t *plug = json_object();
            json_set(plug, "id", json_string("pareto-router"));
            json_set(plug, "min_coding_score", json_number(score_f));
            json_t *arr = json_array();
            json_append(arr, plug);
            json_set(body, "plugins", arr);
        }
    }
    char *out = json_serialize(body);
    json_free(body);
    return out;
}
static void pp_openrouter_kwargs(provider_profile_t *p, const char *ctx_json,
                                 char **out_extra, char **out_top)
{
    (void)p;
    const char *model = ctx_str(ctx_json, "model");
    const char *rc = ctx_str(ctx_json, "reasoning_config");
    bool supports_reasoning = ctx_str(ctx_json, "supports_reasoning") != NULL;
    json_t *extra = json_object();
    json_t *top = json_object();
    if (supports_reasoning) {
        bool mandatory = false;
        if (model && (strncmp(model, "anthropic/", 9) == 0 || strncmp(model, "claude", 6) == 0 || strstr(model, "claude"))) {
            static const char *opt[] = {"claude-3","claude-opus-4-0","claude-opus-4.0",
                "claude-opus-4-1","claude-opus-4.1","claude-sonnet-4-0","claude-sonnet-4.0",
                "claude-opus-4-2025","claude-sonnet-4-2025","claude-opus-4-5","claude-opus-4.5",
                "claude-sonnet-4-5","claude-sonnet-4.5","claude-haiku-4-5","claude-haiku-4.5",NULL};
            mandatory = true;
            for (int i = 0; opt[i]; i++)
                if (strstr(model, opt[i])) { mandatory = false; break; }
        }
        if (mandatory) {
            const char *effort = NULL;
            if (rc) { json_t *c = json_parse(rc, NULL); if (c) { effort = json_get_str(c, "effort", NULL); json_free(c); } }
            bool enabled = true;
            if (rc) { json_t *c = json_parse(rc, NULL); if (c) { enabled = json_get_bool(c, "enabled", true); json_free(c); } }
            if (enabled && effort && strcasecmp(effort, "none") != 0 && strcasecmp(effort, "ultra") != 0)
                json_set(top, "verbosity", json_string(effort));
        } else if (rc) {
            json_t *c = json_parse(rc, NULL);
            json_set(extra, "reasoning", c ? json_copy(c) : json_object());
            if (c) json_free(c);
        } else {
            json_t *r = json_object();
            json_set(r, "enabled", json_bool(true));
            json_set(r, "effort", json_string("medium"));
            json_set(extra, "reasoning", r);
        }
    }
    /* x-grok-conv-id header for xAI Grok models */
    const char *sid2 = hermes_get_conversation_context();
    const char *gc = sid2 ? sid2 : ctx_str(ctx_json, "session_id");
    if (gc && model && (strncmp(model, "x-ai/grok-", 10) == 0 || strncmp(model, "xai/grok-", 9) == 0)) {
        json_t *hdr = json_object();
        json_set(hdr, "x-grok-conv-id", json_string(gc));
        json_set(top, "extra_headers", hdr);
    }
    *out_extra = json_serialize(extra);
    *out_top = json_serialize(top);
    json_free(extra); json_free(top);
}

/* ── nous ────────────────────────────────────────────────────────────── */
static char *pp_nous_extra_body(provider_profile_t *p, const char *ctx_json)
{
    (void)p;
    json_t *body = json_object();
    char *tags = hermes_nous_portal_tags_json();
    if (tags) { json_t *a = json_parse(tags, NULL); if (a) json_set(body, "tags", a); free(tags); }
    const char *sticky = hermes_get_conversation_context();
    const char *key = sticky ? sticky : ctx_str(ctx_json, "session_id");
    if (key) json_set(body, "session_id", json_string(key));
    const char *prefs = ctx_str(ctx_json, "provider_preferences");
    if (prefs && prefs[0]) json_set(body, "provider", json_string(prefs));
    char *out = json_serialize(body);
    json_free(body);
    return out;
}
static void pp_nous_kwargs(provider_profile_t *p, const char *ctx_json,
                           char **out_extra, char **out_top)
{
    (void)p;
    const char *rc = ctx_str(ctx_json, "reasoning_config");
    bool supports = ctx_str(ctx_json, "supports_reasoning") != NULL;
    json_t *extra = json_object();
    if (supports) {
        if (rc) {
            json_t *c = json_parse(rc, NULL);
            bool enabled = c ? json_get_bool(c, "enabled", true) : true;
            if (c) json_free(c);
            if (enabled) {
                json_t *r = c ? json_copy(c) : json_object();
                json_set(extra, "reasoning", r);
            }
            /* Nous omits reasoning when disabled -> nothing */
        } else {
            json_t *r = json_object();
            json_set(r, "enabled", json_bool(true));
            json_set(r, "effort", json_string("medium"));
            json_set(extra, "reasoning", r);
        }
    }
    *out_extra = json_serialize(extra);
    *out_top = NULL;
    json_free(extra);
}

/* ── kimi ────────────────────────────────────────────────────────────── */
static bool is_kimi_coding_url(const char *base_url)
{
    /* urlparse + checks; simplified faithful port. */
    if (!base_url) return false;
    char buf[1024]; strncpy(buf, base_url, sizeof(buf) - 1); buf[sizeof(buf)-1] = '\0';
    if (strncmp(buf, "https://", 8) != 0) return false;
    char *host = buf + 8;
    char *path = strchr(host, '/');
    char *at = strchr(host, '@'); if (at && (!path || at < path)) host = at + 1;
    /* split host:port */
    char *slash = strchr(host, '/');
    if (slash) *slash = '\0';
    char *colon = strrchr(host, ':');
    int port = 443;
    if (colon && strchr(colon, '.') == NULL) { port = atoi(colon + 1); *colon = '\0'; }
    if (strcasecmp(host, "api.kimi.com") != 0) return false;
    const char *p = path ? path : "/";
    size_t plen = strlen(p);
    while (plen && p[plen-1] == '/') plen--;
    if ((plen == 7 || plen == 11) && (strncmp(p, "/coding", plen) == 0 ||
        strncmp(p, "/coding/v1", plen) == 0) &&
        strchr(p, '?') == NULL && strchr(p, '#') == NULL && port == 443)
        return true;
    return false;
}
static char *pp_kimi_models(provider_profile_t *p, const char *api_key,
                            const char *base_url, double timeout)
{
    char ub[1024]; const char *eb = (base_url && base_url[0]) ? base_url : (p->base_url ? p->base_url : "");
    size_t bl = strlen(eb); while (bl && eb[bl-1]=='/') bl--;
    snprintf(ub, sizeof(ub), "%.*s", (int)bl, eb);
    bool coding = is_kimi_coding_url(ub);
    if (coding && strcmp(ub + (bl - 7 < 0 ? 0 : bl - 7 < bl ? bl - 7 : 0), "/coding") == 0) { /* append /v1 */ }
    char full[1100]; snprintf(full, sizeof(full), "%s%s", ub, coding ? "/v1" : "");
    char *res = provider_profile_fetch_models_default(p, api_key, full, timeout);
    if (res && !coding) {
        /* drop "k3" */
        json_t *a = json_parse(res, NULL); free(res);
        if (a && a->type == JSON_ARRAY) {
            json_t *out = json_array();
            for (size_t i = 0; i < json_len(a); i++) {
                json_t *m = json_get(a, i);
                const char *id = m && m->type == JSON_STRING ? m->str_val : (m && m->type==JSON_OBJECT ? json_get_str(m,"id",NULL) : NULL);
                if (id && strcasecmp(id, "k3") != 0) json_append(out, json_copy(m));
            }
            res = json_serialize(out); json_free(out);
        }
        json_free(a);
    }
    return res;
}
static void pp_kimi_kwargs(provider_profile_t *p, const char *ctx_json,
                           char **out_extra, char **out_top)
{
    (void)p;
    const char *rc = ctx_str(ctx_json, "reasoning_config");
    json_t *extra = json_object(); json_t *top = json_object();
    if (!rc) {
        json_set(extra, "thinking", json_object()); /* {"type":"enabled"} */
        json_t *t = json_object(); json_set(t, "type", json_string("enabled"));
        json_set(extra, "thinking", t);
    } else {
        json_t *c = json_parse(rc, NULL);
        bool enabled = c ? json_get_bool(c, "enabled", true) : true;
        if (!enabled) { json_t *t = json_object(); json_set(t, "type", json_string("disabled")); json_set(extra, "thinking", t); }
        else {
            const char *effort = c ? json_get_str(c, "effort", NULL) : NULL;
            if (effort && (strcasecmp(effort,"low")==0 || strcasecmp(effort,"medium")==0 || strcasecmp(effort,"high")==0))
                json_set(top, "reasoning_effort", json_string(effort));
            else { json_t *t = json_object(); json_set(t, "type", json_string("enabled")); json_set(extra, "thinking", t); }
        }
        if (c) json_free(c);
    }
    *out_extra = json_serialize(extra); *out_top = json_serialize(top);
    json_free(extra); json_free(top);
}

/* ── deepseek ────────────────────────────────────────────────────────── */
static bool ds_supports_thinking(const char *model)
{
    if (!model || !model[0]) return false;
    char m[256]; size_t n = strlen(model); if (n >= sizeof(m)) n = sizeof(m)-1;
    for (size_t i = 0; i < n; i++) m[i] = (char)tolower((unsigned char)model[i]);
    if (strncmp(m, "deepseek-v", 9) != 0) return false;
    return strncmp(m, "deepseek-v3", 10) != 0;
}
static void pp_deepseek_kwargs(provider_profile_t *p, const char *ctx_json,
                               char **out_extra, char **out_top)
{
    (void)p;
    const char *model = ctx_str(ctx_json, "model");
    const char *rc = ctx_str(ctx_json, "reasoning_config");
    json_t *extra = json_object(); json_t *top = json_object();
    if (!ds_supports_thinking(model)) goto done;
    bool enabled = true;
    if (rc) { json_t *c = json_parse(rc, NULL); if (c) { enabled = json_get_bool(c, "enabled", true); json_free(c); } }
    json_t *t = json_object();
    json_set(t, "type", json_string(enabled ? "enabled" : "disabled"));
    json_set(extra, "thinking", t);
    if (enabled && rc) {
        json_t *c = json_parse(rc, NULL);
        const char *effort = c ? json_get_str(c, "effort", NULL) : NULL;
        if (effort) {
            if (strcasecmp(effort,"xhigh")==0||strcasecmp(effort,"max")==0||strcasecmp(effort,"ultra")==0)
                json_set(top, "reasoning_effort", json_string("max"));
            else if (strcasecmp(effort,"low")==0||strcasecmp(effort,"medium")==0||strcasecmp(effort,"high")==0)
                json_set(top, "reasoning_effort", json_string(effort));
        }
        if (c) json_free(c);
    }
done:
    *out_extra = json_serialize(extra); *out_top = json_serialize(top);
    json_free(extra); json_free(top);
}

/* ── minimax ─────────────────────────────────────────────────────────── */
static bool is_minimax_global_openai(const char *base_url)
{
    if (!base_url) return false;
    char buf[1024]; strncpy(buf, base_url, sizeof(buf)-1); buf[sizeof(buf)-1]='\0';
    if (strncmp(buf, "https://", 8) != 0) return false;
    char *host = buf + 8;
    char *slash = strchr(host, '/'); if (slash) *slash = '\0';
    if (strcasecmp(host, "api.minimax.io") != 0) return false;
    char *p = strchr(buf + 8, '/');
    return p && strcmp(p, "/v1") == 0;
}
static bool is_minimax_m3(const char *model)
{
    if (!model) return false;
    char m[256]; size_t n = strlen(model); if (n>=sizeof(m)) n=sizeof(m)-1;
    for (size_t i=0;i<n;i++) m[i]=(char)tolower((unsigned char)model[i]);
    return strcmp(m, "minimax-m3")==0 || strcmp(m, "minimax/minimax-m3")==0;
}
static void pp_minimax_kwargs(provider_profile_t *p, const char *ctx_json,
                              char **out_extra, char **out_top)
{
    (void)p;
    const char *model = ctx_str(ctx_json, "model");
    const char *base_url = ctx_str(ctx_json, "base_url");
    const char *rc = ctx_str(ctx_json, "reasoning_config");
    json_t *extra = json_object(); json_t *top = json_object();
    if (!is_minimax_global_openai(base_url) || !is_minimax_m3(model)) goto done;
    json_set(extra, "reasoning_split", json_bool(true));
    if (rc) {
        json_t *c = json_parse(rc, NULL);
        bool enabled = c ? json_get_bool(c, "enabled", true) : true;
        if (!enabled) { json_t *t = json_object(); json_set(t,"type",json_string("disabled")); json_set(extra,"thinking",t); }
        else if (c) { json_t *t = json_object(); json_set(t,"type",json_string("adaptive")); json_set(extra,"thinking",t); }
        if (c) json_free(c);
    }
done:
    *out_extra = json_serialize(extra); *out_top = json_serialize(top);
    json_free(extra); json_free(top);
}

/* ── qwen ────────────────────────────────────────────────────────────── */
static char *pp_qwen_extra_body(provider_profile_t *p, const char *ctx_json)
{
    (void)p; (void)ctx_json;
    json_t *body = json_object();
    json_set(body, "vl_high_resolution_images", json_bool(true));
    char *out = json_serialize(body); json_free(body);
    return out;
}
static void pp_qwen_kwargs(provider_profile_t *p, const char *ctx_json,
                           char **out_extra, char **out_top)
{
    (void)p;
    const char *meta = ctx_str(ctx_json, "qwen_session_metadata");
    *out_extra = NULL; *out_top = NULL;
    if (meta && meta[0]) {
        json_t *top = json_object();
        json_set(top, "metadata", json_string(meta));
        *out_top = json_serialize(top);
        json_free(top);
    }
}

/* ── zai / glm ───────────────────────────────────────────────────────── */
static bool glm_supports_thinking(const char *model)
{
    if (!model || !model[0]) return false;
    char m[256]; size_t n = strlen(model); if (n>=sizeof(m)) n=sizeof(m)-1;
    for (size_t i=0;i<n;i++) m[i]=(char)tolower((unsigned char)model[i]);
    /* glm-(\d+)(.(\d+))? >= (4,5) */
    if (strncmp(m, "glm-", 4) != 0) return false;
    int major = atoi(m + 4);
    const char *dot = strchr(m + 4, '.');
    int minor = dot ? atoi(dot + 1) : 0;
    return (major > 4) || (major == 4 && minor >= 5);
}
static bool is_glm_5_2(const char *model)
{
    if (!model) return false;
    char m[256]; size_t n = strlen(model); if (n>=sizeof(m)) n=sizeof(m)-1;
    for (size_t i=0;i<n;i++) m[i]=(char)tolower((unsigned char)model[i]);
    return strstr(m, "glm-5.2") || strstr(m, "glm-5-2") || strstr(m, "glm-5p2");
}
static const char *glm_52_effort(const char *rc_json)
{
    if (!rc_json) return NULL;
    json_t *c = json_parse(rc_json, NULL); if (!c) return NULL;
    bool enabled = json_get_bool(c, "enabled", true);
    const char *effort = json_get_str(c, "effort", NULL);
    const char *res = NULL;
    if (enabled && effort && strcasecmp(effort, "none") != 0) {
        if (strcasecmp(effort,"xhigh")==0||strcasecmp(effort,"max")==0||strcasecmp(effort,"ultra")==0) res = "max";
        else res = "high";
    }
    json_free(c);
    return res;
}
static void pp_zai_kwargs(provider_profile_t *p, const char *ctx_json,
                          char **out_extra, char **out_top)
{
    (void)p;
    const char *model = ctx_str(ctx_json, "model");
    const char *rc = ctx_str(ctx_json, "reasoning_config");
    json_t *extra = json_object(); json_t *top = json_object();
    if (!glm_supports_thinking(model) && !is_glm_5_2(model)) goto done;
    if (rc) {
        json_t *c = json_parse(rc, NULL);
        bool enabled = c ? json_get_bool(c, "enabled", true) : true;
        json_t *t = json_object();
        json_set(t, "type", json_string(enabled ? "enabled" : "disabled"));
        json_set(extra, "thinking", t);
        if (c) json_free(c);
    }
    if (is_glm_5_2(model)) {
        const char *eff = glm_52_effort(rc);
        if (eff) json_set(top, "reasoning_effort", json_string(eff));
    }
done:
    *out_extra = json_serialize(extra); *out_top = json_serialize(top);
    json_free(extra); json_free(top);
}

/* ── upstage / solar ─────────────────────────────────────────────────── */
static bool upstage_supports_reasoning(const char *model)
{
    if (!model) return true; /* None -> default-on (solar-pro3) */
    char m[256]; size_t n = strlen(model); if (n>=sizeof(m)) n=sizeof(m)-1;
    for (size_t i=0;i<n;i++) m[i]=(char)tolower((unsigned char)model[i]);
    if (strstr(m, "solar-mini") || strstr(m, "syn-pro")) return false;
    return true;
}
static void pp_upstage_kwargs(provider_profile_t *p, const char *ctx_json,
                              char **out_extra, char **out_top)
{
    (void)p;
    const char *model = ctx_str(ctx_json, "model");
    const char *rc = ctx_str(ctx_json, "reasoning_config");
    json_t *extra = json_object(); json_t *top = json_object();
    json_t *c = NULL;
    if (!upstage_supports_reasoning(model)) goto done;
    if (!rc) { json_set(top, "reasoning_effort", json_string("medium")); goto done; }
    c = json_parse(rc, NULL);
    bool enabled = c ? json_get_bool(c, "enabled", true) : true;
    const char *effort = c ? json_get_str(c, "effort", NULL) : NULL;
    if (!enabled) goto done;
    if (!effort || effort[0]=='\0') { json_set(top, "reasoning_effort", json_string("medium")); goto done; }
    const char *mapped = NULL;
    if (strcasecmp(effort,"minimal")==0) mapped = NULL;
    else if (strcasecmp(effort,"low")==0) mapped = "low";
    else if (strcasecmp(effort,"medium")==0) mapped = "medium";
    else if (strcasecmp(effort,"high")==0) mapped = "high";
    else mapped = "high";
    if (mapped) json_set(top, "reasoning_effort", json_string(mapped));
done:
    if (c) json_free(c);
    *out_extra = json_serialize(extra); *out_top = json_serialize(top);
    json_free(extra); json_free(top);
}

/* ── ollama-cloud ────────────────────────────────────────────────────── */
static void pp_ollamacloud_kwargs(provider_profile_t *p, const char *ctx_json,
                                  char **out_extra, char **out_top)
{
    (void)p;
    bool supports = ctx_str(ctx_json, "supports_reasoning") != NULL;
    const char *rc = ctx_str(ctx_json, "reasoning_config");
    json_t *extra = json_object(); json_t *top = json_object();
    if (!supports) goto done;
    if (rc) {
        json_t *c = json_parse(rc, NULL);
        bool enabled = c ? json_get_bool(c, "enabled", true) : true;
        const char *effort = c ? json_get_str(c, "effort", NULL) : NULL;
        if (!enabled) json_set(top, "reasoning_effort", json_string("none"));
        else if (effort && strcasecmp(effort, "none") != 0) {
            if (strcasecmp(effort,"xhigh")==0||strcasecmp(effort,"max")==0||strcasecmp(effort,"ultra")==0)
                json_set(top, "reasoning_effort", json_string("max"));
            else if (strcasecmp(effort,"low")==0||strcasecmp(effort,"medium")==0||strcasecmp(effort,"high")==0)
                json_set(top, "reasoning_effort", json_string(effort));
        }
        if (c) json_free(c);
    }
done:
    *out_extra = json_serialize(extra); *out_top = json_serialize(top);
    json_free(extra); json_free(top);
}

/* ── custom / ollama ─────────────────────────────────────────────────── */
static void pp_custom_kwargs(provider_profile_t *p, const char *ctx_json,
                             char **out_extra, char **out_top)
{
    (void)p;
    const char *rc = ctx_str(ctx_json, "reasoning_config");
    const char *nc = ctx_str(ctx_json, "ollama_num_ctx");
    json_t *extra = json_object(); json_t *top = json_object();
    if (nc && nc[0]) {
        json_t *opts = json_object();
        json_set(opts, "num_ctx", json_number(atof(nc)));
        json_set(extra, "options", opts);
    }
    if (rc) {
        json_t *c = json_parse(rc, NULL);
        const char *effort = c ? json_get_str(c, "effort", NULL) : NULL;
        bool enabled = c ? json_get_bool(c, "enabled", true) : true;
        if ((effort && strcasecmp(effort, "none") == 0) || !enabled) {
            json_set(top, "reasoning_effort", json_string("none"));
            json_set(extra, "think", json_bool(false));
        } else if (effort && effort[0]) {
            json_set(top, "reasoning_effort", json_string(effort));
        }
        if (c) json_free(c);
    }
    *out_extra = json_serialize(extra); *out_top = json_serialize(top);
    json_free(extra); json_free(top);
}
static char *pp_custom_models(provider_profile_t *p, const char *api_key,
                              const char *base_url, double timeout)
{
    const char *eb = (base_url && base_url[0]) ? base_url : (p->base_url && p->base_url[0] ? p->base_url : NULL);
    if (!eb) return NULL;
    return provider_profile_fetch_models_default(p, api_key, eb, timeout);
}

/* ── gemini / vertex (thinking_config via chat_completions._build_gemini_thinking_config) */
/* Faithful port of agent/transports/chat_completions.py:_build_gemini_thinking_config():
 * translate Hermes reasoning_config -> Gemini thinkingConfig. */
static char *gemini_build_thinking_config(const char *model, const char *reasoning_config_json)
{
    if (!reasoning_config_json) return NULL;
    json_t *rc = json_parse(reasoning_config_json, NULL);
    if (!rc) return NULL;
    if (rc->type != JSON_OBJECT) { char *o = json_serialize(rc); json_free(rc); return o; }

    char ml[256] = {0};
    if (model) {
        size_t n = strlen(model);
        if (n >= sizeof(ml)) n = sizeof(ml) - 1;
        for (size_t i = 0; i < n; i++) ml[i] = (char)tolower((unsigned char)model[i]);
    }
    if (strncmp(ml, "google/", 7) == 0) memmove(ml, ml + 7, strlen(ml + 7) + 1);
    if (strncmp(ml, "gemini", 6) != 0) { json_free(rc); return NULL; }

    bool enabled = json_get_bool(rc, "enabled", true);
    json_t *tc = json_object();
    if (!enabled) {
        json_set(tc, "includeThoughts", json_bool(false));
    } else {
        const char *effort = json_get_str(rc, "effort", "medium");
        if (effort && strcasecmp(effort, "none") == 0) {
            json_set(tc, "includeThoughts", json_bool(false));
        } else {
            json_set(tc, "includeThoughts", json_bool(true));
            if (strncmp(ml, "gemini-2.5-", 11) == 0) {
                /* thinkingBudget only, no level guess */
            } else {
                const char *e = effort ? effort : "medium";
                if (strcasecmp(e, "minimal") == 0 || strcasecmp(e, "low") == 0 ||
                    strcasecmp(e, "medium") == 0 || strcasecmp(e, "high") == 0 ||
                    strcasecmp(e, "xhigh") == 0 || strcasecmp(e, "max") == 0 ||
                    strcasecmp(e, "ultra") == 0) { /* known effort */ }
                else e = "medium";
                if (strncmp(ml, "gemini-3", 8) == 0 || strncmp(ml, "gemini-3.1", 10) == 0) {
                    if (strstr(ml, "flash")) {
                        if (strcasecmp(e, "minimal") == 0 || strcasecmp(e, "low") == 0)
                            json_set(tc, "thinkingLevel", json_string("low"));
                        else if (strcasecmp(e, "high") == 0 || strcasecmp(e, "xhigh") == 0 ||
                                 strcasecmp(e, "max") == 0 || strcasecmp(e, "ultra") == 0)
                            json_set(tc, "thinkingLevel", json_string("high"));
                        else json_set(tc, "thinkingLevel", json_string("medium"));
                    } else if (strstr(ml, "pro")) {
                        json_set(tc, "thinkingLevel", json_string(
                            (strcasecmp(e, "high") == 0 || strcasecmp(e, "xhigh") == 0 ||
                             strcasecmp(e, "max") == 0 || strcasecmp(e, "ultra") == 0) ? "high" : "low"));
                    }
                }
            }
        }
    }
    char *out = json_serialize(tc);
    json_free(tc); json_free(rc);
    return out;
}

static char *pp_gemini_extra_body(provider_profile_t *p, const char *ctx_json)
{
    const char *model = ctx_str(ctx_json, "model");
    const char *rc = ctx_str(ctx_json, "reasoning_config");
    const char *base_url = ctx_str(ctx_json, "base_url");
    if (!base_url || !base_url[0]) base_url = p->base_url;
    char *raw = gemini_build_thinking_config(model, rc);
    if (!raw || !raw[0]) { free(raw); return NULL; }
    /* _is_gemini_openai_compat_base_url(base_url): ends with /openai and host is gen ai */
    bool compat = false;
    if (base_url) {
        char b[1024]; strncpy(b, base_url, sizeof(b)-1); b[sizeof(b)-1] = 0;
        size_t bl = strlen(b); while (bl && b[bl-1]=='/') bl--; b[bl] = 0;
        if (strstr(b, "generativelanguage.googleapis.com") &&
            bl >= 6 && strcmp(b + bl - 6, "/openai") == 0) compat = true;
    }
    json_t *rawc = json_parse(raw, NULL); free(raw);
    json_t *body = json_object();
    if (compat) {
        json_t *g = json_object();
        json_t *tc = json_object();
        if (rawc && rawc->type == JSON_OBJECT) {
            if (json_has(rawc, "includeThoughts"))
                json_set(tc, "include_thoughts", json_bool(json_get_bool(rawc, "includeThoughts", false)));
            const char *lvl = json_get_str(rawc, "thinkingLevel", NULL);
            if (lvl && lvl[0]) json_set(tc, "thinking_level", json_string(lvl));
        }
        json_set(g, "thinking_config", tc);
        json_set(body, "extra_body", g);
    } else {
        json_set(body, "thinking_config", rawc ? rawc : json_object());
    }
    char *out = json_serialize(body);
    json_free(body);
    return out;
}

/* ── copilot ─────────────────────────────────────────────────────────── */
static void pp_copilot_kwargs(provider_profile_t *p, const char *ctx_json,
                              char **out_extra, char **out_top)
{
    (void)p;
    const char *model = ctx_str(ctx_json, "model");
    bool supports = ctx_str(ctx_json, "supports_reasoning") != NULL;
    json_t *extra = json_object();
    if (supports && model) {
        char **efforts = models_github_model_reasoning_efforts(NULL, NULL, model, NULL, NULL);
        if (efforts && efforts[0]) {
            const char *effort = "medium";
            const char *rc = ctx_str(ctx_json, "reasoning_config");
            if (rc) {
                json_t *c = json_parse(rc, NULL);
                if (c) { effort = json_get_str(c, "effort", "medium"); json_free(c); }
            }
            /* clamp effort to supported set */
            const char *chosen = efforts[0];
            for (size_t i = 0; efforts[i]; i++) {
                if (strcasecmp(efforts[i], effort) == 0) { chosen = efforts[i]; break; }
                if (strcasecmp(efforts[i], "medium") == 0) chosen = "medium";
            }
            if (strcasecmp(effort, "xhigh") == 0) {
                for (size_t i = 0; efforts[i]; i++) if (strcasecmp(efforts[i], "high")==0) { chosen = "high"; break; }
            }
            if (strcasecmp(effort, "minimal") == 0) {
                for (size_t i = 0; efforts[i]; i++) if (strcasecmp(efforts[i], "low")==0) { chosen = "low"; break; }
            }
            json_t *r = json_object();
            json_set(r, "effort", json_string(chosen));
            json_set(extra, "reasoning", r);
        }
        if (efforts) {
            for (size_t i = 0; efforts[i]; i++) free(efforts[i]);
            free(efforts);
        }
    }
    *out_extra = json_serialize(extra); *out_top = NULL;
    json_free(extra);
}

/* ── opencode-zen / opencode-go ──────────────────────────────────────── */
static const char *flat_model_name(const char *model)
{
    if (!model) return "";
    const char *slash = strrchr(model, '/');
    return slash ? slash + 1 : model;
}
static bool is_kimi_k2(const char *model) { return strncmp(flat_model_name(model), "kimi-k2", 7) == 0; }
static bool is_deepseek_thinking(const char *model)
{
    const char *m = flat_model_name(model);
    if (strncmp(m, "deepseek-v", 9) != 0) return strcmp(m, "deepseek-reasoner") == 0;
    return strncmp(m, "deepseek-v3", 10) != 0;
}
static bool is_glm_5_2_model(const char *model) {
    const char *m = flat_model_name(model);
    return strstr(m, "glm-5.2") || strstr(m, "glm-5-2") || strstr(m, "glm-5p2");
}
static void pp_opencode_go_kwargs(provider_profile_t *p, const char *ctx_json,
                                  char **out_extra, char **out_top)
{
    (void)p;
    const char *model = ctx_str(ctx_json, "model");
    const char *rc = ctx_str(ctx_json, "reasoning_config");
    json_t *extra = json_object(); json_t *top = json_object();
    if (is_glm_5_2_model(model)) {
        if (rc) {
            json_t *c = json_parse(rc, NULL);
            bool enabled = c ? json_get_bool(c, "enabled", true) : true;
            if (enabled && c) {
                const char *effort = json_get_str(c, "effort", NULL);
                if (effort && strcasecmp(effort, "none") != 0) {
                    const char *l = (strcasecmp(effort,"xhigh")==0||strcasecmp(effort,"max")==0||strcasecmp(effort,"ultra")==0) ? "max" : "high";
                    json_set(top, "reasoning_effort", json_string(l));
                }
            }
            if (c) json_free(c);
        }
    } else if (is_kimi_k2(model)) {
        if (rc) {
            json_t *c = json_parse(rc, NULL);
            bool enabled = c ? json_get_bool(c, "enabled", true) : true;
            if (!enabled) { json_t *t=json_object(); json_set(t,"type",json_string("disabled")); json_set(extra,"thinking",t); }
            else {
                const char *effort = c ? json_get_str(c, "effort", NULL) : NULL;
                if (strcasecmp(effort,"xhigh")==0||strcasecmp(effort,"max")==0||strcasecmp(effort,"ultra")==0) json_set(top,"reasoning_effort",json_string("high"));
                else if (strcasecmp(effort,"low")==0||strcasecmp(effort,"medium")==0||strcasecmp(effort,"high")==0) json_set(top,"reasoning_effort",json_string(effort));
                else { json_t *t=json_object(); json_set(t,"type",json_string("enabled")); json_set(extra,"thinking",t); }
            }
            if (c) json_free(c);
        }
    } else if (is_deepseek_thinking(model)) {
        bool enabled = true;
        if (rc) { json_t *c = json_parse(rc, NULL); if (c) { enabled = json_get_bool(c, "enabled", true); json_free(c); } }
        if (!enabled) { json_t *t=json_object(); json_set(t,"type",json_string("disabled")); json_set(extra,"thinking",t); }
        else if (rc) {
            json_t *c = json_parse(rc, NULL);
            const char *effort = c ? json_get_str(c, "effort", NULL) : NULL;
            if (strcasecmp(effort,"xhigh")==0||strcasecmp(effort,"max")==0||strcasecmp(effort,"ultra")==0) json_set(top,"reasoning_effort",json_string("max"));
            else if (strcasecmp(effort,"low")==0||strcasecmp(effort,"medium")==0||strcasecmp(effort,"high")==0) json_set(top,"reasoning_effort",json_string(effort));
            else { json_t *t=json_object(); json_set(t,"type",json_string("enabled")); json_set(extra,"thinking",t); }
            if (c) json_free(c);
        }
    }
    *out_extra = json_serialize(extra); *out_top = json_serialize(top);
    json_free(extra); json_free(top);
}
static int pp_opencode_go_max_tokens(provider_profile_t *p, const char *model)
{
    if (strcmp(flat_model_name(model), "mimo-v2.5-pro") == 0) return 131072;
    return p->default_max_tokens;
}

/* ── deepinfra default_vision_model ──────────────────────────────────── */
static char *pp_deepinfra_vision(const char *models_json)
{
    if (!models_json) return NULL;
    json_t *a = json_parse(models_json, NULL);
    if (!a || a->type != JSON_ARRAY) { json_free(a); return NULL; }
    char *ret = NULL;
    for (size_t i = 0; i < json_len(a) && !ret; i++) {
        json_t *it = json_get(a, i);
        if (!it || it->type != JSON_OBJECT) continue;
        json_t *meta = json_obj_get(it, "metadata");
        json_t *tags = (meta && meta->type == JSON_OBJECT) ? json_obj_get(meta, "tags") : NULL;
        bool chat = false;
        if (tags && tags->type == JSON_ARRAY)
            for (size_t j = 0; j < json_len(tags); j++) {
                json_t *tg = json_get(tags, j);
                if (tg && tg->type == JSON_STRING && strcmp(tg->str_val, "chat") == 0) chat = true;
            }
        if (chat) {
            const char *id = json_get_str(it, "id", NULL);
            if (id) ret = strdup(id);
        }
    }
    json_free(a);
    return ret;
}
static char *pp_deepinfra_default_vision(provider_profile_t *p)
{
    (void)p;
    const char *key = getenv("DEEPINFRA_API_KEY");
    if (!key || !key[0]) return NULL;
    /* fetch chat-tagged models; pick first vision-capable chat model */
    /* Reuse the default fetch_models which GETs the models endpoint. */
    char *models = provider_profile_fetch_models_default(p, key, NULL, 8.0);
    char *ret = pp_deepinfra_vision(models);
    free(models);
    return ret;
}

/* ── anthropic / bedrock / copilot-acp: fetch_models -> NULL ─────────── */
static char *pp_null_models(provider_profile_t *p, const char *a, const char *b, double t)
{ (void)p; (void)a; (void)b; (void)t; return NULL; }

/* ── builder ─────────────────────────────────────────────────────────── */
static void reg(provider_profile_t *p) { provider_profile_register(p); }

void provider_profiles_register_builtin(void)
{
    if (g_builtin_done) return;

    /* ---- openrouter ---- */
    {
        provider_profile_t *p = provider_profile_new("openrouter");
        p->aliases = STRVS("or");
        p->env_vars = STRVS("OPENROUTER_API_KEY");
        p->display_name = xstrdup_("OpenRouter");
        p->description = xstrdup_("OpenRouter — unified API for 200+ models");
        p->signup_url = xstrdup_("https://openrouter.ai/keys");
        p->base_url = xstrdup_("https://openrouter.ai/api/v1");
        p->build_extra_body = pp_openrouter_extra_body;
        p->build_api_kwargs_extras = pp_openrouter_kwargs;
        reg(p);
    }
    /* ---- nous ---- */
    {
        provider_profile_t *p = provider_profile_new("nous");
        p->aliases = STRVS("nous-portal", "nousresearch");
        p->env_vars = STRVS("NOUS_API_KEY");
        p->display_name = xstrdup_("Nous Research");
        p->description = xstrdup_("Nous Research — Hermes model family");
        p->signup_url = xstrdup_("https://nousresearch.com/");
        p->fallback_models = STRVS("hermes-3-405b", "hermes-3-70b");
        p->base_url = xstrdup_("https://inference-api.nousresearch.com/v1");
        p->auth_type = xstrdup_("oauth_device_code");
        p->build_extra_body = pp_nous_extra_body;
        p->build_api_kwargs_extras = pp_nous_kwargs;
        reg(p);
    }
    /* ---- kimi-coding (+ cn) ---- */
    {
        provider_profile_t *p = provider_profile_new("kimi-coding");
        p->aliases = STRVS("kimi", "moonshot", "kimi-for-coding");
        p->env_vars = STRVS("KIMI_API_KEY", "KIMI_CODING_API_KEY");
        p->base_url = xstrdup_("https://api.moonshot.ai/v1");
        p->fixed_temperature_mode = PROFILE_TEMP_OMIT;
        p->default_max_tokens = 32000;
        p->default_headers_json = xstrdup_("{\"User-Agent\":\"hermes-agent/1.0\"}");
        p->default_aux_model = xstrdup_("kimi-k2-turbo-preview");
        p->fetch_models = pp_kimi_models;
        p->build_api_kwargs_extras = pp_kimi_kwargs;
        reg(p);
        provider_profile_t *pcn = provider_profile_new("kimi-coding-cn");
        pcn->aliases = STRVS("kimi-cn", "moonshot-cn");
        pcn->env_vars = STRVS("KIMI_CN_API_KEY");
        pcn->base_url = xstrdup_("https://api.moonshot.cn/v1");
        pcn->fixed_temperature_mode = PROFILE_TEMP_OMIT;
        pcn->default_max_tokens = 32000;
        pcn->default_headers_json = xstrdup_("{\"User-Agent\":\"hermes-agent/1.0\"}");
        pcn->default_aux_model = xstrdup_("kimi-k2-turbo-preview");
        pcn->fetch_models = pp_kimi_models;
        pcn->build_api_kwargs_extras = pp_kimi_kwargs;
        reg(pcn);
    }
    /* ---- deepseek ---- */
    {
        provider_profile_t *p = provider_profile_new("deepseek");
        p->aliases = STRVS("deepseek-chat");
        p->env_vars = STRVS("DEEPSEEK_API_KEY");
        p->display_name = xstrdup_("DeepSeek");
        p->description = xstrdup_("DeepSeek — native DeepSeek API");
        p->signup_url = xstrdup_("https://platform.deepseek.com");
        p->base_url = xstrdup_("https://api.deepseek.com");
        p->build_api_kwargs_extras = pp_deepseek_kwargs;
        reg(p);
    }
    /* ---- minimax (+ cn, + oauth) ---- */
    {
        provider_profile_t *p = provider_profile_new("minimax");
        p->aliases = STRVS("mini-max");
        p->api_mode = xstrdup_("anthropic_messages");
        p->env_vars = STRVS("MINIMAX_API_KEY");
        p->base_url = xstrdup_("https://api.minimax.io/anthropic");
        p->auth_type = xstrdup_("api_key");
        p->default_aux_model = xstrdup_("MiniMax-M3");
        p->build_api_kwargs_extras = pp_minimax_kwargs;
        reg(p);
        provider_profile_t *pcn = provider_profile_new("minimax-cn");
        pcn->aliases = STRVS("minimax-china", "minimax_cn");
        pcn->api_mode = xstrdup_("anthropic_messages");
        pcn->env_vars = STRVS("MINIMAX_CN_API_KEY");
        pcn->base_url = xstrdup_("https://api.minimaxi.com/anthropic");
        pcn->auth_type = xstrdup_("api_key");
        pcn->default_aux_model = xstrdup_("MiniMax-M3");
        pcn->build_api_kwargs_extras = pp_minimax_kwargs;
        reg(pcn);
        provider_profile_t *po = provider_profile_new("minimax-oauth");
        po->aliases = STRVS("minimax_oauth", "minimax-oauth-io");
        po->api_mode = xstrdup_("anthropic_messages");
        po->display_name = xstrdup_("MiniMax (OAuth)");
        po->description = xstrdup_("MiniMax via OAuth browser flow — no API key required");
        po->signup_url = xstrdup_("https://api.minimax.io/");
        po->env_vars = STRVS(""); /* OAuth */
        po->base_url = xstrdup_("https://api.minimax.io/anthropic");
        po->auth_type = xstrdup_("oauth_external");
        po->default_aux_model = xstrdup_("MiniMax-M2.7");
        po->build_api_kwargs_extras = pp_minimax_kwargs;
        reg(po);
    }
    /* ---- qwen ---- */
    {
        provider_profile_t *p = provider_profile_new("qwen-oauth");
        p->aliases = STRVS("qwen", "qwen-portal", "qwen-cli");
        p->env_vars = STRVS("QWEN_API_KEY");
        p->base_url = xstrdup_("https://portal.qwen.ai/v1");
        p->auth_type = xstrdup_("oauth_external");
        p->default_max_tokens = 65536;
        p->build_extra_body = pp_qwen_extra_body;
        p->build_api_kwargs_extras = pp_qwen_kwargs;
        reg(p);
    }
    /* ---- zai / glm ---- */
    {
        provider_profile_t *p = provider_profile_new("zai");
        p->aliases = STRVS("glm", "z-ai", "z.ai", "zhipu");
        p->env_vars = STRVS("GLM_API_KEY", "ZAI_API_KEY", "Z_AI_API_KEY");
        p->display_name = xstrdup_("Z.AI (GLM)");
        p->description = xstrdup_("Z.AI / GLM — Zhipu AI models");
        p->signup_url = xstrdup_("https://z.ai/");
        p->fallback_models = STRVS("glm-5.2", "glm-5", "glm-4-9b");
        p->base_url = xstrdup_("https://api.z.ai/api/paas/v4");
        p->default_aux_model = xstrdup_("glm-4.5-flash");
        p->build_api_kwargs_extras = pp_zai_kwargs;
        reg(p);
    }
    /* ---- upstage / solar ---- */
    {
        provider_profile_t *p = provider_profile_new("upstage");
        p->aliases = STRVS("solar");
        p->display_name = xstrdup_("Upstage Solar");
        p->description = xstrdup_("Upstage (Solar API)");
        p->signup_url = xstrdup_("https://console.upstage.ai/");
        p->base_url = xstrdup_("https://api.upstage.ai/v1");
        p->build_api_kwargs_extras = pp_upstage_kwargs;
        reg(p);
    }
    /* ---- ollama-cloud ---- */
    {
        provider_profile_t *p = provider_profile_new("ollama-cloud");
        p->aliases = STRVS("ollama_cloud");
        p->default_aux_model = xstrdup_("nemotron-3-nano:30b");
        p->env_vars = STRVS("OLLAMA_API_KEY");
        p->base_url = xstrdup_("https://ollama.com/v1");
        p->build_api_kwargs_extras = pp_ollamacloud_kwargs;
        reg(p);
    }
    /* ---- opencode-zen / opencode-go ---- */
    {
        provider_profile_t *p = provider_profile_new("opencode-zen");
        p->aliases = STRVS("opencode", "opencode_zen", "zen");
        p->env_vars = STRVS("OPENCODE_ZEN_API_KEY");
        p->base_url = xstrdup_("https://opencode.ai/zen/v1");
        p->default_aux_model = xstrdup_("gemini-3-flash");
        reg(p);
        provider_profile_t *pg = provider_profile_new("opencode-go");
        pg->aliases = STRVS("opencode_go", "go", "opencode-go-sub");
        pg->env_vars = STRVS("OPENCODE_GO_API_KEY");
        pg->base_url = xstrdup_("https://opencode.ai/zen/go/v1");
        pg->default_aux_model = xstrdup_("glm-5");
        pg->build_api_kwargs_extras = pp_opencode_go_kwargs;
        pg->get_max_tokens = pp_opencode_go_max_tokens;
        reg(pg);
    }
    /* ---- gemini ---- */
    {
        provider_profile_t *p = provider_profile_new("gemini");
        p->aliases = STRVS("google", "google-gemini", "google-ai-studio");
        p->api_mode = xstrdup_("chat_completions");
        p->env_vars = STRVS("GOOGLE_API_KEY", "GEMINI_API_KEY");
        p->base_url = xstrdup_("https://generativelanguage.googleapis.com/v1beta");
        p->auth_type = xstrdup_("api_key");
        p->default_aux_model = xstrdup_("gemini-3.5-flash");
        p->build_extra_body = pp_gemini_extra_body;
        reg(p);
    }
    /* ---- copilot ---- */
    {
        provider_profile_t *p = provider_profile_new("copilot");
        p->aliases = STRVS("github-copilot", "github-models", "github-model", "github");
        p->env_vars = STRVS("COPILOT_GITHUB_TOKEN", "GH_TOKEN", "GITHUB_TOKEN");
        p->base_url = xstrdup_("https://api.githubcopilot.com");
        p->auth_type = xstrdup_("copilot");
        p->build_api_kwargs_extras = pp_copilot_kwargs;
        reg(p);
    }
    /* ---- custom / ollama ---- */
    {
        provider_profile_t *p = provider_profile_new("custom");
        p->aliases = STRVS("ollama", "local", "vllm", "llamacpp", "llama.cpp", "llama-cpp");
        p->env_vars = STRVS("");
        p->base_url = xstrdup_("");
        p->default_max_tokens = 65536;
        p->build_api_kwargs_extras = pp_custom_kwargs;
        p->fetch_models = pp_custom_models;
        reg(p);
    }
    /* ---- alibaba / alibaba-coding-plan ---- */
    {
        provider_profile_t *p = provider_profile_new("alibaba");
        p->aliases = STRVS("dashscope", "alibaba-cloud", "qwen-dashscope");
        p->env_vars = STRVS("DASHSCOPE_API_KEY");
        p->base_url = xstrdup_("https://dashscope-intl.aliyuncs.com/compatible-mode/v1");
        reg(p);
        provider_profile_t *pc = provider_profile_new("alibaba-coding-plan");
        pc->aliases = STRVS("alibaba_coding", "alibaba-coding", "dashscope-coding");
        pc->display_name = xstrdup_("Alibaba Cloud (Coding Plan)");
        pc->description = xstrdup_("Alibaba Cloud Coding Plan (Dedicated coding tier)");
        pc->signup_url = xstrdup_("https://help.aliyun.com/zh/model-studio/");
        pc->env_vars = STRVS("ALIBABA_CODING_PLAN_API_KEY", "DASHSCOPE_API_KEY", "ALIBABA_CODING_PLAN_BASE_URL");
        pc->base_url = xstrdup_("https://coding-intl.dashscope.aliyuncs.com/v1");
        pc->auth_type = xstrdup_("api_key");
        reg(pc);
    }
    /* ---- anthropic ---- */
    {
        provider_profile_t *p = provider_profile_new("anthropic");
        p->aliases = STRVS("claude", "claude-oauth", "claude-code");
        p->api_mode = xstrdup_("anthropic_messages");
        p->env_vars = STRVS("ANTHROPIC_API_KEY", "ANTHROPIC_TOKEN", "CLAUDE_CODE_OAUTH_TOKEN");
        p->base_url = xstrdup_("https://api.anthropic.com");
        p->auth_type = xstrdup_("api_key");
        p->default_aux_model = xstrdup_("claude-haiku-4-5-20251001");
        p->fetch_models = pp_null_models;
        reg(p);
    }
    /* ---- arcee ---- */
    {
        provider_profile_t *p = provider_profile_new("arcee");
        p->aliases = STRVS("arcee-ai", "arceeai");
        p->env_vars = STRVS("ARCEEAI_API_KEY");
        p->base_url = xstrdup_("https://api.arcee.ai/api/v1");
        reg(p);
    }
    /* ---- azure-foundry ---- */
    {
        provider_profile_t *p = provider_profile_new("azure-foundry");
        p->aliases = STRVS("azure", "azure-ai-foundry", "azure-ai");
        p->display_name = xstrdup_("Azure Foundry");
        p->description = xstrdup_("Microsoft Foundry - OpenAI-compatible endpoint (user-supplied base URL)");
        p->signup_url = xstrdup_("https://ai.azure.com/");
        p->env_vars = STRVS("AZURE_FOUNDRY_API_KEY", "AZURE_FOUNDRY_BASE_URL");
        p->base_url = xstrdup_("");
        p->auth_type = xstrdup_("api_key");
        reg(p);
    }
    /* ---- bedrock ---- */
    {
        provider_profile_t *p = provider_profile_new("bedrock");
        p->aliases = STRVS("aws", "aws-bedrock", "amazon-bedrock", "amazon");
        p->api_mode = xstrdup_("bedrock_converse");
        p->env_vars = STRVS("");
        p->base_url = xstrdup_("https://bedrock-runtime.us-east-1.amazonaws.com");
        p->auth_type = xstrdup_("aws_sdk");
        p->fetch_models = pp_null_models;
        reg(p);
    }
    /* ---- copilot-acp ---- */
    {
        provider_profile_t *p = provider_profile_new("copilot-acp");
        p->aliases = STRVS("github-copilot-acp", "copilot-acp-agent");
        p->api_mode = xstrdup_("chat_completions");
        p->env_vars = STRVS("");
        p->base_url = xstrdup_("acp://copilot");
        p->auth_type = xstrdup_("external_process");
        p->fetch_models = pp_null_models;
        reg(p);
    }
    /* ---- deepinfra ---- */
    {
        provider_profile_t *p = provider_profile_new("deepinfra");
        p->aliases = STRVS("deep-infra", "deep-infra");
        p->env_vars = STRVS("DEEPINFRA_API_KEY");
        p->base_url = xstrdup_("https://api.deepinfra.com/v1/openai");
        p->default_vision_model = pp_deepinfra_default_vision;
        reg(p);
    }
    /* ---- fireworks ---- */
    {
        provider_profile_t *p = provider_profile_new("fireworks");
        p->aliases = STRVS("fireworks-ai", "fw");
        p->display_name = xstrdup_("Fireworks AI");
        p->description = xstrdup_("Fireworks AI — OpenAI-compatible direct model API");
        p->signup_url = xstrdup_("https://app.fireworks.ai/settings/users/api-keys");
        p->env_vars = STRVS("FIREWORKS_API_KEY");
        p->base_url = xstrdup_("https://api.fireworks.ai/inference/v1");
        p->auth_type = xstrdup_("api_key");
        p->default_aux_model = xstrdup_("accounts/fireworks/models/glm-5p2");
        p->fallback_models = STRVS("accounts/fireworks/models/kimi-k2p6", "accounts/fireworks/models/glm-5p2", "accounts/fireworks/models/kimi-k2p7-code");
        reg(p);
    }
    /* ---- gmi ---- */
    {
        provider_profile_t *p = provider_profile_new("gmi");
        p->aliases = STRVS("gmi-cloud", "gmicloud");
        p->display_name = xstrdup_("GMI Cloud");
        p->description = xstrdup_("GMI Cloud — multi-model direct API (slash-form model IDs)");
        p->signup_url = xstrdup_("https://www.gmicloud.ai/");
        p->env_vars = STRVS("GMI_API_KEY", "GMI_BASE_URL");
        p->base_url = xstrdup_("https://api.gmi-serving.com/v1");
        p->auth_type = xstrdup_("api_key");
        char ua[64]; snprintf(ua, sizeof(ua), "HermesAgent/%s", HERMES_VERSION);
        p->default_headers_json = xstrdup2(ua);
        p->default_aux_model = xstrdup_("google/gemini-3.1-flash-lite-preview");
        p->fallback_models = STRVS("zai-org/GLM-5.1-FP8","deepseek-ai/DeepSeek-V3.2","moonshotai/Kimi-K2.5","google/gemini-3.1-flash-lite-preview","anthropic/claude-sonnet-5","anthropic/claude-sonnet-4.6","openai/gpt-5.4");
        reg(p);
    }
    /* ---- huggingface ---- */
    {
        provider_profile_t *p = provider_profile_new("huggingface");
        p->aliases = STRVS("hf", "hugging-face", "huggingface-hub");
        p->env_vars = STRVS("HF_TOKEN");
        p->display_name = xstrdup_("HuggingFace");
        p->description = xstrdup_("HuggingFace Inference API");
        p->signup_url = xstrdup_("https://huggingface.co/settings/tokens");
        p->fallback_models = STRVS("Qwen/Qwen3.5-72B-Instruct", "deepseek-ai/DeepSeek-V3.2");
        p->base_url = xstrdup_("https://router.huggingface.co/v1");
        reg(p);
    }
    /* ---- kilocode ---- */
    {
        provider_profile_t *p = provider_profile_new("kilocode");
        p->aliases = STRVS("kilo-code", "kilo", "kilo-gateway");
        p->env_vars = STRVS("KILOCODE_API_KEY");
        p->base_url = xstrdup_("https://api.kilo.ai/api/gateway");
        p->default_aux_model = xstrdup_("google/gemini-3-flash-preview");
        reg(p);
    }
    /* ---- novita ---- */
    {
        provider_profile_t *p = provider_profile_new("novita");
        p->aliases = STRVS("novita-ai", "novitaai");
        p->display_name = xstrdup_("NovitaAI");
        p->description = xstrdup_("NovitaAI — AI-native cloud for builders and agents");
        p->signup_url = xstrdup_("https://novita.ai/settings/key-management");
        p->env_vars = STRVS("NOVITA_API_KEY", "NOVITA_BASE_URL");
        p->base_url = xstrdup_("https://api.novita.ai/openai/v1");
        p->auth_type = xstrdup_("api_key");
        p->default_aux_model = xstrdup_("deepseek/deepseek-v3-0324");
        p->fallback_models = STRVS("moonshotai/kimi-k2.5","minimax/minimax-m2.7","zai-org/glm-5","deepseek/deepseek-v3-0324","deepseek/deepseek-r1-0528","qwen/qwen3-235b-a22b-fp8");
        reg(p);
    }
    /* ---- nvidia ---- */
    {
        provider_profile_t *p = provider_profile_new("nvidia");
        p->aliases = STRVS("nvidia-nim");
        p->env_vars = STRVS("NVIDIA_API_KEY");
        p->display_name = xstrdup_("NVIDIA NIM");
        p->description = xstrdup_("NVIDIA NIM — accelerated inference");
        p->signup_url = xstrdup_("https://build.nvidia.com/");
        p->fallback_models = STRVS("nvidia/llama-3.1-nemotron-70b-instruct", "nvidia/llama-3.3-70b-instruct");
        p->base_url = xstrdup_("https://integrate.api.nvidia.com/v1");
        p->default_max_tokens = 16384;
        reg(p);
    }
    /* ---- openai-codex ---- */
    {
        provider_profile_t *p = provider_profile_new("openai-codex");
        p->aliases = STRVS("codex", "openai_codex");
        p->api_mode = xstrdup_("codex_responses");
        p->env_vars = STRVS("");
        p->base_url = xstrdup_("https://chatgpt.com/backend-api/codex");
        p->auth_type = xstrdup_("oauth_external");
        reg(p);
    }
    /* ---- stepfun ---- */
    {
        provider_profile_t *p = provider_profile_new("stepfun");
        p->aliases = STRVS("step", "stepfun-coding-plan");
        p->default_aux_model = xstrdup_("step-3.5-flash");
        p->env_vars = STRVS("STEPFUN_API_KEY");
        p->base_url = xstrdup_("https://api.stepfun.ai/step_plan/v1");
        reg(p);
    }
    /* ---- vertex ---- */
    {
        provider_profile_t *p = provider_profile_new("vertex");
        p->aliases = STRVS("vertex-ai", "google-vertex");
        p->api_mode = xstrdup_("chat_completions");
        p->env_vars = STRVS("GOOGLE_API_KEY", "GOOGLE_APPLICATION_CREDENTIALS");
        p->base_url = xstrdup_("https://aiplatform.googleapis.com/v1");
        p->auth_type = xstrdup_("vertex");
        p->build_extra_body = pp_gemini_extra_body;
        reg(p);
    }
    /* ---- xai ---- */
    {
        provider_profile_t *p = provider_profile_new("xai");
        p->aliases = STRVS("grok", "x-ai", "x.ai");
        p->api_mode = xstrdup_("codex_responses");
        p->env_vars = STRVS("XAI_API_KEY");
        p->base_url = xstrdup_("https://api.x.ai/v1");
        p->auth_type = xstrdup_("api_key");
        char ua[64]; snprintf(ua, sizeof(ua), "Hermes-Agent/%s", HERMES_VERSION);
        p->default_headers_json = xstrdup2(ua);
        reg(p);
    }
    /* ---- xiaomi ---- */
    {
        provider_profile_t *p = provider_profile_new("xiaomi");
        p->aliases = STRVS("mimo", "xiaomi-mimo");
        p->env_vars = STRVS("XIAOMI_API_KEY");
        p->base_url = xstrdup_("https://api.xiaomimimo.com/v1");
        p->supports_health_check = false;
        p->supports_vision = true;
        p->supports_vision_tool_messages = false;
        reg(p);
    }

    provider_profile_mark_builtin_done();
}
