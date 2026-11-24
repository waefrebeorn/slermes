/*
 * runtime_provider_backends.c -- real C11 port of
 * hermes_cli/runtime_provider_backends.py (gate w23_runtimepro_54c1e6).
 *
 * PoP: w23_runtimepro_54c1e6_azure_foundry @ hermes_cli/runtime_provider_backends.py:_resolve_azure_foundry_runtime
 * PoP: w23_runtimepro_54c1e6_openrouter @ hermes_cli/runtime_provider_backends.py:_resolve_openrouter_runtime
 * PoP: w23_runtimepro_54c1e6_bedrock @ hermes_cli/runtime_provider_backends.py:_resolve_bedrock_runtime
 * PoP: w23_runtimepro_54c1e6_is_external @ hermes_cli/runtime_provider_backends.py:_is_external_process_provider
 * PoP: w23_runtimepro_54c1e6_external_runtime @ hermes_cli/runtime_provider_backends.py:_resolve_external_process_runtime
 *
 * Upstream keeps its origin-internal collaborators on
 * `hermes_cli.runtime_provider` (and the auth / runtime_provider_custom
 * helpers re-exported there) and resolves them per call through `_rp()` "so
 * test patches ... still apply". Those reads are the seam: `world.rp` answers
 * them. The decisions this module owns -- base_url precedence, the `/v1`
 * strip, auth_mode normalization, the Entra vs API-key branch, the source
 * label, key candidate order and usability, the alias->custom remap, the
 * OpenRouter-context test, the pool short-circuit, the placeholder key, and
 * Bedrock's triple-path routing -- are implemented here.
 */
#include "hermes_json.h"
#include "noc_w23_runtimepro_54c1e6.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RPB54_OPENROUTER_BASE_URL "https://openrouter.ai/api/v1"
#define RPB54_NO_KEY "no-key-required"
#define RPB54_AWS_DEFAULT_CHAIN "aws-sdk-default-chain"
#define RPB54_AZURE_NO_KEY \
    "Azure Foundry requires an API key. Set AZURE_FOUNDRY_API_KEY in " \
    "~/.hermes/.env or run 'hermes model' to configure. To use " \
    "keyless Microsoft Entra ID auth instead, set " \
    "model.auth_mode: entra_id in config.yaml (or pick " \
    "'Microsoft Entra ID' in 'hermes model')."
#define RPB54_AZURE_NO_URL \
    "Azure Foundry requires a base URL. Set it via 'hermes model' or " \
    "the AZURE_FOUNDRY_BASE_URL environment variable."
#define RPB54_AWS_NO_CREDS \
    "No AWS credentials found for Bedrock. Configure one of:\n" \
    "  - AWS_ACCESS_KEY_ID + AWS_SECRET_ACCESS_KEY\n" \
    "  - AWS_PROFILE (for SSO / named profiles)\n" \
    "  - IAM instance role (EC2, ECS, Lambda)\n" \
    "Or run 'aws configure' to set up credentials."

/* -- Python scalar/str helpers ------------------------------------------- */

static char *rpb54_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

static int rpb54_space(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

/* Python str.strip() for the ASCII shapes config carries. */
static char *rpb54_strip(const char *s) {
    size_t b = 0, e;
    char *out;
    if (!s) return rpb54_dup("");
    e = strlen(s);
    while (b < e && rpb54_space((unsigned char)s[b])) b++;
    while (e > b && rpb54_space((unsigned char)s[e - 1])) e--;
    out = (char *)malloc(e - b + 1);
    if (!out) return NULL;
    memcpy(out, s + b, e - b);
    out[e - b] = '\0';
    return out;
}

static void rpb54_lower(char *s) {
    for (; s && *s; s++) s[0] = (char)tolower((unsigned char)*s);
}

/* str.rstrip(char) as a fresh string. */
static char *rpb54_rstrip_char(const char *s, char ch) {
    size_t n;
    char *out;
    if (!s) return rpb54_dup("");
    n = strlen(s);
    while (n > 0 && s[n - 1] == ch) n--;
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

/* Python truthiness of a JSON value. */
static int rpb54_truth(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL: return 0;
        case JSON_BOOL: return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
    }
    return 0;
}

static void rpb54_num(double x, char *buf, size_t cap) {
    int prec;
    if (x == floor(x) && fabs(x) < 1e16) {
        snprintf(buf, cap, "%.0f", x);
        return;
    }
    for (prec = 15; prec <= 17; prec++) {
        snprintf(buf, cap, "%.*g", prec, x);
        if (strtod(buf, NULL) == x) return;
    }
    snprintf(buf, cap, "%.17g", x);
}

/* Python str() of a JSON scalar (containers are out of the fixture scope). */
static char *rpb54_pystr(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return rpb54_dup("None");
    if (v->type == JSON_BOOL) return rpb54_dup(v->bool_val ? "True" : "False");
    if (v->type == JSON_STRING) return rpb54_dup(v->str_val ? v->str_val : "");
    if (v->type == JSON_NUMBER) {
        rpb54_num(v->num_val, buf, sizeof(buf));
        return rpb54_dup(buf);
    }
    return rpb54_dup("");
}

static const json_t *rpb54_obj(const json_t *v) {
    return (v && v->type == JSON_OBJECT) ? v : NULL;
}

/* `str(v or "")`: absent/None/empty reads as "". */
static char *rpb54_opt_str(const json_t *v) {
    if (!rpb54_truth(v)) return rpb54_dup("");
    if (v->type == JSON_STRING) return rpb54_dup(v->str_val ? v->str_val : "");
    return rpb54_pystr(v);
}

/* `str(v or "").strip()`. */
static char *rpb54_str_strip(const json_t *v) {
    char *raw = rpb54_opt_str(v);
    char *out = rpb54_strip(raw);
    free(raw);
    return out;
}

/* `str(v or "").strip().rstrip("/")`. */
static char *rpb54_str_url(const json_t *v) {
    char *stripped = rpb54_str_strip(v);
    char *out = rpb54_rstrip_char(stripped, '/');
    free(stripped);
    return out;
}

/* `world.rp` = the origin-module answers. */
static const json_t *rpb54_rp(const json_t *w) {
    return rpb54_obj(json_obj_get(w, "rp"));
}

static const char *rpb54_wrs(const json_t *obj, const char *key, const char *def) {
    return json_get_str(obj, key, def);
}

static int rpb54_wrb(const json_t *obj, const char *key, int def) {
    return json_get_bool(obj, key, def ? true : false) ? 1 : 0;
}

/* `get_secret_str(name, "")` / `get_env_value(name)` stand-ins. */
static const char *rpb54_secret(const json_t *w, const char *name) {
    return json_get_str(rpb54_obj(json_obj_get(w, "secrets")), name, "");
}

static const char *rpb54_dotenv(const json_t *w, const char *name) {
    return json_get_str(rpb54_obj(json_obj_get(w, "dotenv")), name, "");
}

/* -- `auth.has_usable_secret` / `auth.looks_like_openrouter_key` ---------- */

static const char *const RPB54_PLACEHOLDERS[] = {
    "*", "**", "***", "changeme", "your_api_key", "your_api_key_here", "your-api-key",
    "placeholder", "example", "dummy", "null", "none"
};

static int rpb54_all_x(const char *s) {
    if (!s || !*s) return 0;
    for (; *s; s++) if (*s != 'x') return 0;
    return 1;
}

static int rpb54_placeholder_shape(const char *cleaned) {
    size_t n = strlen(cleaned), i, j = 0;
    char *flat;
    int all;
    if (n >= 6 && !strncmp(cleaned, "your_", 5) && !strcmp(cleaned + n - 5, "_here")) return 1;
    if (!strncmp(cleaned, "sk-", 3) && rpb54_all_x(cleaned + 3)) return 1;
    if (!strncmp(cleaned, "ghp_", 4) && rpb54_all_x(cleaned + 4)) return 1;
    if (!strncmp(cleaned, "hf_", 3) && rpb54_all_x(cleaned + 3)) return 1;
    flat = (char *)malloc(n + 1);
    if (!flat) return 0;
    for (i = 0; i < n; i++) {
        char c = cleaned[i];
        if (c == ' ' || c == '-' || c == '_') continue;
        flat[j++] = c;
    }
    flat[j] = '\0';
    all = rpb54_all_x(flat);
    free(flat);
    return all;
}

static int rpb54_usable(const json_t *v) {
    char *cleaned, *lowered;
    size_t i, n;
    int ok = 1;
    if (!v || v->type != JSON_STRING) return 0;
    cleaned = rpb54_strip(v->str_val);
    if (!cleaned) return 0;
    n = strlen(cleaned);
    if (n < 4) { free(cleaned); return 0; }
    lowered = rpb54_dup(cleaned);
    if (!lowered) { free(cleaned); return 0; }
    rpb54_lower(lowered);
    for (i = 0; i < sizeof(RPB54_PLACEHOLDERS) / sizeof(RPB54_PLACEHOLDERS[0]); i++) {
        if (!strcmp(lowered, RPB54_PLACEHOLDERS[i])) { ok = 0; break; }
    }
    if (ok && rpb54_placeholder_shape(cleaned)) ok = 0;
    free(lowered);
    free(cleaned);
    return ok;
}

/* `looks_like_openrouter_key(value)`: only an sk-or- value may be read as an
 * OpenRouter credential. */
static int rpb54_openrouter_shaped(const char *value) {
    char *s = rpb54_strip(value ? value : "");
    int ok = s && !strncmp(s, "sk-or-", 6);
    free(s);
    return ok ? 1 : 0;
}

/* -- urlparse-lite: hostname / origin of a provider base URL ------------- */

typedef struct {
    char scheme[16];
    char host[256];
    int has_host;
    int bad_port;
    int port;
} rpb54_url_t;

static void rpb54_url_parse(const char *base_url, rpb54_url_t *out) {
    char *raw, *rest, *auth, *slash, *at, *colon;
    size_t i;
    memset(out, 0, sizeof(*out));
    raw = rpb54_strip(base_url ? base_url : "");
    if (!raw || !*raw) { free(raw); return; }
    rest = strstr(raw, "://");
    if (rest) {
        size_t slen = (size_t)(rest - raw);
        if (slen >= sizeof(out->scheme)) slen = sizeof(out->scheme) - 1;
        for (i = 0; i < slen; i++) out->scheme[i] = (char)tolower((unsigned char)raw[i]);
        out->scheme[slen] = '\0';
        rest += 3;
    } else {
        rest = raw;
        while (*rest == '/') rest++;
    }
    auth = rest;
    slash = strpbrk(auth, "/?#");
    if (slash) *slash = '\0';
    at = strrchr(auth, '@');
    if (at) auth = at + 1;
    if (*auth == '[') {
        char *close = strchr(auth, ']');
        if (close) {
            size_t hlen = (size_t)(close - auth - 1);
            if (hlen >= sizeof(out->host)) hlen = sizeof(out->host) - 1;
            memcpy(out->host, auth + 1, hlen);
            out->host[hlen] = '\0';
            for (i = 0; i < hlen; i++) out->host[i] = (char)tolower((unsigned char)out->host[i]);
            out->has_host = hlen > 0;
            colon = strchr(close, ':');
            if (colon) {
                if (!isdigit((unsigned char)colon[1])) out->bad_port = 1;
                else out->port = atoi(colon + 1);
            }
        }
    } else {
        colon = strrchr(auth, ':');
        if (colon) {
            *colon = '\0';
            if (!*auth || !isdigit((unsigned char)colon[1])) out->bad_port = 1;
            else out->port = atoi(colon + 1);
        }
        if (*auth) {
            size_t hlen = strlen(auth);
            if (hlen >= sizeof(out->host)) hlen = sizeof(out->host) - 1;
            memcpy(out->host, auth, hlen);
            out->host[hlen] = '\0';
            while (hlen > 0 && out->host[hlen - 1] == '.') out->host[--hlen] = '\0';
            for (i = 0; i < hlen; i++) out->host[i] = (char)tolower((unsigned char)out->host[i]);
            out->has_host = hlen > 0;
        } else {
            out->bad_port = 1;
        }
    }
    if (out->has_host) {
        if (out->port == 0)
            out->port = !strcmp(out->scheme, "https") ? 443 : (!strcmp(out->scheme, "http") ? 80 : 0);
        if (out->port < 1 || out->port > 65535) out->bad_port = 1;
    }
    free(raw);
}

/* `base_url_host_matches(base_url, domain)`. */
static int rpb54_host_matches(const char *base_url, const char *domain) {
    rpb54_url_t u;
    size_t hl, dl;
    rpb54_url_parse(base_url, &u);
    if (!u.has_host) return 0;
    hl = strlen(u.host);
    dl = strlen(domain);
    if (!hl || !dl) return 0;
    if (!strcmp(u.host, domain)) return 1;
    return hl > dl && !strcmp(u.host + hl - dl, domain) && u.host[hl - dl - 1] == '.';
}

/* -- the runtime dict (`rp._runtime`) ------------------------------------ */

static json_t *rpb54_runtime(const json_t *w, const char *provider, const char *api_mode,
                             const char *base_url, json_t *api_key, json_t *extra) {
    const json_t *rp = rpb54_rp(w);
    const char *mode = api_mode ? api_mode : "";
    const char *url = base_url ? base_url : "";
    json_t *out = json_object();
    if (rpb54_wrb(rp, "actual_route", 0)) {
        const char *actual = rpb54_wrs(rp, "actual_url", "");
        mode = "chat_completions";
        if (actual[0]) url = actual;
    }
    json_set(out, "provider", json_string(provider ? provider : ""));
    json_set(out, "api_mode", json_string(mode));
    json_set(out, "base_url", json_string(url));
    json_set(out, "api_key", api_key ? api_key : json_null());
    if (extra) {
        size_t i;
        for (i = 0; i < json_object_size(extra); i++) {
            const char *k = json_object_get_key_at(extra, i);
            json_t *v = json_object_get_at(extra, i);
            if (k) json_set(out, k, json_copy(v));
        }
        json_free(extra);
    }
    return out;
}

static json_t *rpb54_ok(json_t *runtime) {
    json_t *out = json_object();
    json_set(out, "status", json_string("ok"));
    json_set(out, "runtime", runtime ? runtime : json_null());
    return out;
}

static json_t *rpb54_err(const char *type, const char *message, const char *code) {
    json_t *out = json_object();
    json_set(out, "status", json_string("error"));
    json_set(out, "type", json_string(type ? type : "Exception"));
    json_set(out, "message", json_string(message ? message : ""));
    if (code && code[0]) json_set(out, "code", json_string(code));
    return out;
}

/* -- Azure Foundry ------------------------------------------------------- */

/* `_azure_entra_credentials`. The adapter is an SDK seam; the provider names
 * the scope it mints for, so the scope decision stays visible. `*owned` is a
 * malloc'd refusal message the caller frees. */
static char *rpb54_entra_creds(const json_t *w, const json_t *cfg_entra, char **owned) {
    const json_t *azure = rpb54_obj(json_obj_get(w, "azure"));
    const char *adapter = rpb54_wrs(azure, "adapter", "ok");
    char *scope, *out;
    size_t cap;
    if (!strcmp(adapter, "fail")) {
        const char *msg = rpb54_wrs(azure, "fail_msg", "");
        cap = strlen(msg) + 200;
        out = (char *)malloc(cap);
        if (out) {
            snprintf(out, cap,
                     "Could not load the Azure Foundry Entra ID adapter. "
                     "Run hermes pm repair, then restart Hermes. "
                     "(import failed: %s)", msg);
            *owned = out;
        }
        return NULL;
    }
    scope = rpb54_str_strip(json_obj_get(cfg_entra, "scope"));
    if (!scope || !scope[0]) {
        free(scope);
        scope = rpb54_dup(rpb54_wrs(azure, "default_scope", ""));
    }
    if (!strcmp(adapter, "import_error")) {
        *owned = rpb54_dup(rpb54_wrs(azure, "import_error_msg", ""));
        free(scope);
        return NULL;
    }
    cap = strlen(scope) + 8;
    out = (char *)malloc(cap);
    if (out) snprintf(out, cap, "token:%s", scope);
    free(scope);
    return out;
}

/* `_azure_foundry_api_key`: explicit, else `.env`, else the secret scope. */
static char *rpb54_azure_api_key(const json_t *w, const char *explicit_key, const char **err) {
    char *key;
    if (explicit_key && explicit_key[0]) return rpb54_dup(explicit_key);
    key = rpb54_dup(rpb54_dotenv(w, "AZURE_FOUNDRY_API_KEY"));
    if (!key || !key[0]) {
        free(key);
        key = rpb54_strip(rpb54_secret(w, "AZURE_FOUNDRY_API_KEY"));
    }
    if (!key || !key[0]) {
        free(key);
        *err = RPB54_AZURE_NO_KEY;
        return NULL;
    }
    return key;
}

json_t *w23_runtimepro_54c1e6_azure_foundry(const json_t *w) {
    const json_t *rp = rpb54_rp(w);
    const json_t *mc = rpb54_obj(json_obj_get(w, "model_cfg"));
    const json_t *explicit_api = json_obj_get(w, "explicit_api_key");
    const json_t *explicit_base = json_obj_get(w, "explicit_base_url");
    const json_t *req = json_obj_get(w, "requested_provider");
    int is_tp = rpb54_wrb(w, "explicit_is_token_provider", 0);
    char *explicit_key = NULL, *forwarded = NULL, *explicit_clean = NULL;
    char *cfg_base = NULL, *cfg_mode = NULL, *auth_mode = NULL;
    char *effective = NULL, *env_base = NULL, *base_url = NULL, *api_key = NULL;
    char *owned_msg = NULL;
    const json_t *cfg_entra = NULL;
    const char *err = NULL;
    const char *source;
    json_t *extra, *rt = NULL;

    if (is_tp) {
        forwarded = rpb54_opt_str(explicit_api);
        explicit_key = rpb54_dup("");
    } else {
        explicit_key = rpb54_str_strip(explicit_api);
        forwarded = NULL;
    }
    explicit_clean = rpb54_str_url(explicit_base);
    cfg_base = rpb54_dup("");
    cfg_mode = rpb54_dup("chat_completions");
    auth_mode = rpb54_dup("api_key");
    if (!strcmp(rpb54_wrs(rp, "cfg_provider", ""), "azure-foundry")) {
        const char *parsed;
        char *trimmed;
        free(cfg_base);
        cfg_base = rpb54_dup(rpb54_wrs(rp, "cfg_base_url", ""));
        parsed = rpb54_wrs(rp, "parse_api_mode", "");
        free(cfg_mode);
        cfg_mode = rpb54_dup(parsed[0] ? parsed : "chat_completions");
        trimmed = rpb54_str_strip(json_obj_get(mc, "auth_mode"));
        rpb54_lower(trimmed);
        free(auth_mode);
        if (trimmed && trimmed[0]) {
            auth_mode = trimmed;
        } else {
            free(trimmed);
            auth_mode = rpb54_dup("api_key");
        }
        if (rpb54_obj(json_obj_get(mc, "entra"))) cfg_entra = json_obj_get(mc, "entra");
    }
    {
        char *raw = rpb54_str_strip(json_obj_get(w, "target_model"));
        if (!raw[0]) {
            free(raw);
            raw = rpb54_str_strip(json_obj_get(mc, "default"));
        }
        effective = raw;
    }
    {
        const char *inferred = rpb54_wrs(rp, "azure_inferred", "");
        if (inferred[0]) {
            free(cfg_mode);
            cfg_mode = rpb54_dup(inferred);
        }
    }
    {
        char *trimmed = rpb54_strip(rpb54_secret(w, "AZURE_FOUNDRY_BASE_URL"));
        env_base = rpb54_rstrip_char(trimmed, '/');
        free(trimmed);
    }
    if (explicit_clean[0]) base_url = rpb54_dup(explicit_clean);
    else if (cfg_base[0]) base_url = rpb54_dup(cfg_base);
    else base_url = rpb54_dup(env_base);
    if (!base_url[0]) {
        rt = rpb54_err("AuthError", RPB54_AZURE_NO_URL, NULL);
        goto done;
    }
    if (!strcmp(cfg_mode, "anthropic_messages")) {
        size_t n = strlen(base_url);
        if (n >= 4 && !strcmp(base_url + n - 4, "/v1/")) base_url[n - 4] = '\0';
        else if (n >= 3 && !strcmp(base_url + n - 3, "/v1")) base_url[n - 3] = '\0';
    }
    extra = json_object();
    json_set(extra, "requested_provider", req ? json_copy(req) : json_null());
    if (!strcmp(auth_mode, "entra_id")) {
        if (explicit_key[0]) {
            /* --api-key on the CLI while config says entra_id: honour the
             * explicit string, as an api_key runtime. */
            api_key = rpb54_dup(explicit_key);
            source = "explicit";
            json_set(extra, "auth_mode", json_string("api_key"));
            json_set(extra, "entra", json_object());
        } else {
            char *scope = rpb54_str_strip(cfg_entra ? json_obj_get(cfg_entra, "scope") : NULL);
            char *creds;
            if (forwarded && forwarded[0]) {
                creds = rpb54_dup(forwarded);
            } else {
                creds = rpb54_entra_creds(w, cfg_entra, &owned_msg);
                if (!creds) {
                    json_free(extra);
                    rt = rpb54_err("AuthError", owned_msg ? owned_msg : "", NULL);
                    free(scope);
                    goto done;
                }
            }
            api_key = creds;
            source = "entra_id";
            json_set(extra, "auth_mode", json_string("entra_id"));
            {
                json_t *entra = json_object();
                if (scope && scope[0]) json_set(entra, "scope", json_string(scope));
                json_set(extra, "entra", entra);
            }
            free(scope);
        }
        json_set(extra, "source", json_string(source));
        rt = rpb54_ok(rpb54_runtime(w, "azure-foundry", cfg_mode, base_url,
                                    json_string(api_key ? api_key : ""), extra));
        goto done;
    }
    api_key = rpb54_azure_api_key(w, explicit_key, &err);
    if (!api_key) {
        json_free(extra);
        rt = rpb54_err("AuthError", err ? err : "", NULL);
        goto done;
    }
    source = (explicit_key[0] || rpb54_truth(explicit_base)) ? "explicit" : "config";
    json_set(extra, "auth_mode", json_string("api_key"));
    json_set(extra, "source", json_string(source));
    rt = rpb54_ok(rpb54_runtime(w, "azure-foundry", cfg_mode, base_url,
                                json_string(api_key ? api_key : ""), extra));

done:
    free(explicit_key);
    free(forwarded);
    free(explicit_clean);
    free(cfg_base);
    free(cfg_mode);
    free(auth_mode);
    free(effective);
    free(env_base);
    free(base_url);
    free(api_key);
    free(owned_msg);
    return rt ? rt : rpb54_err("Exception", "unresolved", NULL);
}

/* -- OpenRouter / bare custom -------------------------------------------- */

/* `str(c or "").strip()` for one candidate. */
static char *rpb54_cand_text(const json_t *c) {
    char *raw = rpb54_opt_str(c);
    char *out = rpb54_strip(raw);
    free(raw);
    return out;
}

json_t *w23_runtimepro_54c1e6_openrouter(const json_t *w) {
    const json_t *rp = rpb54_rp(w);
    const json_t *mc = rpb54_obj(json_obj_get(w, "model_cfg"));
    const json_t *explicit_api = json_obj_get(w, "explicit_api_key");
    const json_t *explicit_base = json_obj_get(w, "explicit_base_url");
    const json_t *req = json_obj_get(w, "requested_provider");
    char *cfg_base_url = NULL, *cfg_provider = NULL, *cfg_api_key = NULL;
    char *requested = NULL, *env_or = NULL, *env_custom = NULL, *base_url = NULL;
    char *api_key = NULL, *cfg_mode = NULL, *mode = NULL;
    int use_config = 0, is_or_url = 0, is_or_ctx = 0, owned_route = 0, declared = 0;
    const char *entry_key = "";
    const char *source;
    json_t *cands = NULL, *extra = NULL, *rt = NULL;
    size_t i;

    {
        const json_t *v = json_obj_get(mc, "base_url");
        cfg_base_url = rpb54_dup((v && v->type == JSON_STRING) ? v->str_val : "");
    }
    {
        const json_t *v = json_obj_get(mc, "provider");
        cfg_provider = rpb54_strip((v && v->type == JSON_STRING) ? v->str_val : "");
        rpb54_lower(cfg_provider);
    }
    {
        const char *names[2] = {"api_key", "api"};
        cfg_api_key = rpb54_dup("");
        for (i = 0; i < 2; i++) {
            const json_t *v = json_obj_get(mc, names[i]);
            if (v && v->type == JSON_STRING) {
                char *trimmed = rpb54_strip(v->str_val);
                if (trimmed && trimmed[0]) {
                    free(cfg_api_key);
                    cfg_api_key = trimmed;
                    break;
                }
                free(trimmed);
            }
        }
    }
    requested = rpb54_str_strip(req);
    rpb54_lower(requested);
    /* Aliases resolving to "custom" (ollama, vllm, ...) follow bare-custom
     * trust + routing rules. */
    if (requested[0] && strcmp(requested, "custom") && rpb54_wrb(rp, "resolves_to_custom", 0)) {
        free(requested);
        requested = rpb54_dup("custom");
    }
    env_or = rpb54_strip(rpb54_secret(w, "OPENROUTER_BASE_URL"));
    env_custom = rpb54_strip(rpb54_secret(w, "CUSTOM_BASE_URL"));
    {
        char *cfg_stripped = rpb54_strip(cfg_base_url);
        int clause = 0;
        if (!strcmp(requested, "auto") && (!cfg_provider[0] || !strcmp(cfg_provider, "auto")))
            clause = 1;
        else if (!strcmp(requested, "custom") && rpb54_wrb(rp, "trustworthy", 0))
            clause = 1;
        else if (!strcmp(requested, "openrouter") && !strcmp(cfg_provider, "openrouter"))
            clause = 1;
        use_config = (cfg_stripped && cfg_stripped[0]) && !rpb54_truth(explicit_base) && clause;
        free(cfg_stripped);
    }
    {
        char *explicit_clean = rpb54_str_url(explicit_base);
        char *cfg_pick = NULL;
        const char *pick = NULL;
        if (explicit_clean[0]) pick = explicit_clean;
        else if (env_custom[0]) pick = env_custom;
        else if (use_config) {
            cfg_pick = rpb54_strip(cfg_base_url);
            if (cfg_pick[0]) pick = cfg_pick;
        }
        if (!pick && env_or[0]) pick = env_or;
        if (!pick) pick = RPB54_OPENROUTER_BASE_URL;
        base_url = rpb54_rstrip_char(pick, '/');
        free(cfg_pick);
        free(explicit_clean);
    }
    is_or_url = rpb54_host_matches(base_url, "openrouter.ai");
    if (is_or_url) {
        is_or_ctx = 1;
    } else if (!strcmp(requested, "openrouter")) {
        /* An explicitly-configured OpenRouter mirror still counts as
         * OpenRouter for key selection (#10622). */
        char *cfg_norm = rpb54_strip(cfg_base_url);
        char *cfg_trim = rpb54_rstrip_char(cfg_norm, '/');
        char *env_trim = rpb54_rstrip_char(env_or, '/');
        int cond = 0;
        if (use_config && !strcmp(cfg_provider, "openrouter") && !strcmp(base_url, cfg_trim)) {
            cond = 1;
        } else {
            int left = env_or[0] || !strcmp(base_url, env_or);
            if (left && !strcmp(base_url, env_trim)) cond = 1;
        }
        is_or_ctx = cond;
        free(cfg_norm);
        free(cfg_trim);
        free(env_trim);
    }
    /* A `providers:` entry that owns the resolved URL and declares a
     * credential supplies it. */
    owned_route = !strcmp(requested, "custom") || (is_or_ctx && !is_or_url);
    if (owned_route) {
        const json_t *owned = rpb54_obj(json_obj_get(rp, "owned"));
        declared = rpb54_wrb(owned, "declared", 0);
        entry_key = rpb54_wrs(owned, "key", "");
    }
    cands = json_array();
    json_append(cands, explicit_api ? json_copy(explicit_api) : json_null());
    if (is_or_ctx) {
        /* Same read order as the credential pool: `.env` first, and
         * OPENAI_API_KEY only where it may go. */
        const char *openai_key = rpb54_dotenv(w, "OPENAI_API_KEY");
        char *openai_base = rpb54_strip(rpb54_secret(w, "OPENAI_BASE_URL"));
        rpb54_url_t ob, bu;
        int key_ok;
        rpb54_url_parse(openai_base, &ob);
        if (ob.has_host) {
            rpb54_url_parse(base_url, &bu);
            key_ok = !ob.bad_port && !bu.bad_port && !strcmp(ob.scheme, bu.scheme) &&
                     !strcmp(ob.host, bu.host) && ob.port == bu.port;
        } else {
            key_ok = !is_or_url || rpb54_openrouter_shaped(openai_key);
        }
        if (!declared) {
            json_append(cands, json_string(rpb54_dotenv(w, "OPENROUTER_API_KEY")));
            json_append(cands, json_string(key_ok ? openai_key : ""));
        }
        free(openai_base);
    } else {
        if (use_config) {
            json_append(cands, json_string(cfg_api_key));
            json_append(cands, json_string(rpb54_wrs(rp, "key_env_for", "")));
        }
        if (!declared) {
            const json_t *gated = json_obj_get(rp, "host_gated");
            if (gated && gated->type == JSON_ARRAY) {
                for (i = 0; i < json_len(gated); i++) {
                    json_t *it = json_get(gated, i);
                    json_append(cands, it ? json_copy(it) : json_null());
                }
            }
        }
    }
    for (i = 0; i < json_len(cands); i++) {
        json_t *c = json_get(cands, i);
        if (rpb54_usable(c)) {
            api_key = rpb54_cand_text(c);
            break;
        }
    }
    if (!api_key) api_key = rpb54_dup("");
    if (!api_key[0] && entry_key[0]) {
        free(api_key);
        api_key = rpb54_dup(entry_key);
    }
    source = (rpb54_truth(explicit_api) || rpb54_truth(explicit_base)) ? "explicit" : "env/config";
    cfg_mode = rpb54_dup(rpb54_wrs(rp, "parse_api_mode", ""));
    /* Explicit "custom" stays "custom" rather than relabeling to
     * "openrouter". */
    if (strcmp(requested, "custom")) {
        if (cfg_mode[0]) {
            mode = rpb54_dup(cfg_mode);
        } else {
            const char *detected = rpb54_wrs(rp, "detect_api_mode", "");
            mode = rpb54_dup(detected[0] ? detected : "chat_completions");
        }
        extra = json_object();
        json_set(extra, "source", json_string(source));
        rt = rpb54_ok(rpb54_runtime(w, "openrouter", mode, base_url,
                                    json_string(api_key), extra));
        goto done;
    }
    /* An explicit launch key is the operator's credential for this route and
     * wins over the pool (R2-M1). */
    if (base_url[0] && !rpb54_usable(explicit_api)) {
        const json_t *pool = rpb54_obj(json_obj_get(rp, "pool_result"));
        if (pool) {
            rt = rpb54_ok(json_copy(pool));
            goto done;
        }
    }
    /* Local no-auth servers get a placeholder key: the OpenAI SDK requires a
     * non-empty string. */
    if (!api_key[0] && !is_or_url) {
        free(api_key);
        api_key = rpb54_dup(RPB54_NO_KEY);
    }
    {
        const char *plain = rpb54_wrs(rp, "plain_custom_api_mode", "chat_completions");
        extra = json_object();
        json_set(extra, "source", json_string(source));
        rt = rpb54_ok(rpb54_runtime(w, "custom", plain, base_url, json_string(api_key), extra));
    }

done:
    json_free(cands);
    free(cfg_base_url);
    free(cfg_provider);
    free(cfg_api_key);
    free(requested);
    free(env_or);
    free(env_custom);
    free(base_url);
    free(api_key);
    free(cfg_mode);
    free(mode);
    return rt ? rt : rpb54_err("Exception", "unresolved", NULL);
}

/* -- AWS Bedrock --------------------------------------------------------- */

static int rpb54_bedrock_explicit(const char *provider) {
    static const char *const names[] = {"bedrock", "aws", "aws-bedrock", "amazon-bedrock", "amazon"};
    size_t i;
    for (i = 0; i < sizeof(names) / sizeof(names[0]); i++)
        if (provider && !strcmp(provider, names[i])) return 1;
    return 0;
}

json_t *w23_runtimepro_54c1e6_bedrock(const json_t *w) {
    const json_t *aws = rpb54_obj(json_obj_get(w, "aws"));
    const json_t *mc = rpb54_obj(json_obj_get(w, "model_cfg"));
    const json_t *req = json_obj_get(w, "requested_provider");
    const json_t *guardrail = rpb54_obj(json_obj_get(aws, "guardrail"));
    char *requested = rpb54_str_strip(req);
    char *region = NULL, *auth_source = NULL, *current_model = NULL, *base = NULL;
    int has_bearer;
    json_t *extra = NULL, *rt = NULL;

    if (!rpb54_bedrock_explicit(requested) && !rpb54_wrb(aws, "has_credentials", 0)) {
        rt = rpb54_err("AuthError", RPB54_AWS_NO_CREDS, "no_aws_credentials");
        free(requested);
        return rt;
    }
    region = rpb54_dup(rpb54_wrs(aws, "region", "us-east-1"));
    {
        const char *raw_source = rpb54_wrs(aws, "auth_env_var", "");
        auth_source = rpb54_dup(raw_source[0] ? raw_source : RPB54_AWS_DEFAULT_CHAIN);
    }
    {
        char *raw = rpb54_str_strip(json_obj_get(w, "target_model"));
        if (!raw[0]) {
            free(raw);
            raw = rpb54_str_strip(json_obj_get(mc, "default"));
        }
        current_model = raw;
    }
    {
        char *bearer_env = rpb54_strip(rpb54_wrs(aws, "env_bearer", ""));
        has_bearer = (bearer_env && bearer_env[0]) ? 1 : 0;
        free(bearer_env);
    }
    {
        size_t cap = strlen(region) + 64;
        base = (char *)malloc(cap);
        if (base) snprintf(base, cap, "https://bedrock-runtime.%s.amazonaws.com", region);
    }
    extra = json_object();
    json_set(extra, "source", json_string(auth_source));
    json_set(extra, "region", json_string(region));
    json_set(extra, "requested_provider", req ? json_copy(req) : json_null());
    rt = rpb54_ok(rpb54_runtime(w, "bedrock", "bedrock_converse", base,
                                json_string("aws-sdk"), extra));
    extra = NULL;
    if (rpb54_wrb(aws, "mantle", 0)) {
        const char *bearer = rpb54_wrs(aws, "bearer", "");
        json_t *runtime = json_obj_get(rt, "runtime");
        size_t cap = strlen(region) + 64;
        char *mantle_url = (char *)malloc(cap);
        if (mantle_url)
            snprintf(mantle_url, cap, "https://bedrock-mantle.%s.api.aws/openai/v1", region);
        json_set(runtime, "api_mode", json_string("codex_responses"));
        json_set(runtime, "base_url", json_string(mantle_url ? mantle_url : ""));
        json_set(runtime, "api_key", json_string(bearer[0] ? bearer : "aws-sdk"));
        json_set(runtime, "source",
                 json_string(bearer[0] ? "AWS_BEARER_TOKEN_BEDROCK" : auth_source));
        json_set(runtime, "model", json_string(current_model));
        json_set(runtime, "bedrock_openai", json_bool(true));
        free(mantle_url);
    } else if (rpb54_wrb(aws, "anthropic", 0) && !has_bearer) {
        /* AWS_BEARER_TOKEN_BEDROCK auth is unsupported by AnthropicBedrock
         * (SigV4 only): bearer users go through Converse regardless. */
        json_t *runtime = json_obj_get(rt, "runtime");
        json_set(runtime, "api_mode", json_string("anthropic_messages"));
        json_set(runtime, "bedrock_anthropic", json_bool(true));
    }
    if (guardrail && json_object_size(guardrail) > 0) {
        json_set(json_obj_get(rt, "runtime"), "guardrail_config", json_copy(guardrail));
    }
    free(requested);
    free(region);
    free(auth_source);
    free(current_model);
    free(base);
    return rt;
}

/* -- external-process providers ------------------------------------------ */

/* 1 when the table has the name, 0 when it does not, -1 when the registry
 * read itself raised (upstream catches that and falls through). */
static int rpb54_table_has(const json_t *table, const char *name, int table_error) {
    if (table_error) return -1;
    return rpb54_obj(json_obj_get(table, name)) != NULL ? 1 : 0;
}

json_t *w23_runtimepro_54c1e6_is_external(const json_t *w) {
    char *name = rpb54_strip(rpb54_wrs(w, "provider", ""));
    const json_t *table;
    const json_t *entry;
    int found;
    json_t *out;
    if (!name || !name[0]) { free(name); return json_bool(false); }
    rpb54_lower(name);
    table = rpb54_obj(json_obj_get(w, "registry"));
    found = rpb54_table_has(table, name, rpb54_wrb(w, "registry_error", 0));
    if (found == 1) {
        entry = rpb54_obj(json_obj_get(table, name));
        out = json_bool(!strcmp(rpb54_wrs(entry, "auth_type", ""), "external_process"));
        free(name);
        return out;
    }
    /* The profile registry answers before the CLI registry has been extended. */
    table = rpb54_obj(json_obj_get(w, "profiles"));
    found = rpb54_table_has(table, name, rpb54_wrb(w, "profiles_error", 0));
    if (found != 1) { free(name); return json_bool(false); }
    entry = rpb54_obj(json_obj_get(table, name));
    out = json_bool(!strcmp(rpb54_wrs(entry, "auth_type", ""), "external_process"));
    free(name);
    return out;
}

json_t *w23_runtimepro_54c1e6_external_runtime(const json_t *w) {
    const json_t *creds = rpb54_obj(json_obj_get(w, "creds"));
    const json_t *req = json_obj_get(w, "requested_provider");
    const json_t *base_node = json_obj_get(creds, "base_url");
    const json_t *api_node = json_obj_get(creds, "api_key");
    const json_t *args_node = json_obj_get(creds, "args");
    char *provider = rpb54_opt_str(json_obj_get(w, "provider"));
    char *base = NULL;
    json_t *extra, *args = json_array(), *rt;

    if (!base_node) {
        base = rpb54_dup("");
    } else if (base_node->type == JSON_STRING) {
        base = rpb54_rstrip_char(base_node->str_val, '/');
    } else {
        const char *type_name = base_node->type == JSON_NUMBER ? "int"
                                : (base_node->type == JSON_BOOL ? "bool"
                                : (base_node->type == JSON_NULL ? "NoneType" : "dict"));
        size_t cap = strlen(type_name) + 64;
        char *msg = (char *)malloc(cap);
        if (msg) snprintf(msg, cap, "'%s' object has no attribute 'rstrip'", type_name);
        rt = rpb54_err("AttributeError", msg ? msg : "", NULL);
        free(msg);
        free(provider);
        json_free(args);
        return rt;
    }
    {
        const char *src = rpb54_wrs(creds, "source", "process");
        const char *cmd = rpb54_wrs(creds, "command", "");
        size_t i;
        if (args_node && args_node->type == JSON_ARRAY) {
            for (i = 0; i < json_len(args_node); i++) {
                json_t *it = json_get(args_node, i);
                json_append(args, it ? json_copy(it) : json_null());
            }
        } else if (args_node && args_node->type == JSON_STRING) {
            for (i = 0; args_node->str_val && args_node->str_val[i]; i++) {
                char ch[2];
                ch[0] = args_node->str_val[i];
                ch[1] = '\0';
                json_append(args, json_string(ch));
            }
        } else if (args_node && args_node->type == JSON_NUMBER) {
            rt = rpb54_err("TypeError", "'int' object is not iterable", NULL);
            free(base);
            free(provider);
            json_free(args);
            return rt;
        }
        extra = json_object();
        json_set(extra, "command", json_string(cmd));
        json_set(extra, "args", args);
        args = NULL;
        json_set(extra, "source", json_string(src));
        json_set(extra, "requested_provider", req ? json_copy(req) : json_null());
    }
    rt = rpb54_ok(rpb54_runtime(w, provider, "chat_completions", base,
                                api_node ? json_copy(api_node) : json_string(""), extra));
    {
        const json_t *pnode = json_obj_get(w, "provider");
        if (!pnode || pnode->type == JSON_NULL) {
            json_set(json_obj_get(rt, "runtime"), "provider", json_null());
        }
    }
    free(base);
    free(provider);
    return rt;
}
