/*
 * tool_backend_helpers.c — Name parity wrapper for Python tools/tool_backend_helpers.py
 *
 * NOTE: The C implementation lives in lib/libtoolbackend/tool_backend.c, not here.
 * This file exists ONLY for name parity so that every Python module
 * has a correspondingly-named C file.
 *
 * Port of Python tools/tool_backend_helpers.py.
 * C implementation: lib/libtoolbackend/tool_backend.c
 *
 * Shared helpers for tool backend selection
 *
 * Key functions ported:
 *   tool_backend_select, tool_backend_available, tool_backend_resolve
 */
#include "hermes_core_types.h"

/* === merged from port_tools_tool_backend_helpers.c === */
/*
 * port_tools_tool_backend_helpers.c — C port of tools/tool_backend_helpers.py
 */

#include "hermes_logger.h"
#include "credential_pool.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_tools_tool_backend_helpers_nous_tool_gateway_unavailable_message @ tools/tool_backend_helpers.py:nous_tool_gateway_unavailable_message */

/* Port of Python tools/tool_backend_helpers.py:nous_tool_gateway_unavailable_message */
/* Return account-aware guidance for an unavailable Nous Tool Gateway path. */
char *cli_tools_tool_backend_helpers_nous_tool_gateway_unavailable_message(
    const char *capability)
{
    if (!capability) capability = "the Nous Tool Gateway";

    /* In a full implementation, this would:
     * 1. Check Nous Portal account info
     * 2. Format entitlement message
     * 3. Return account-specific guidance
     *
     * For now, return a generic message.
     */
    size_t buf_size = strlen(capability) + 256;
    char *msg = (char *)malloc(buf_size);
    if (!msg) return NULL;

    snprintf(msg, buf_size,
        "%s is unavailable. Run `hermes model` to refresh your "
        "Nous Portal login and billing status.",
        capability);

    return msg;
}

/* PoP: cli_tools_tool_backend_helpers_fal_key_is_configured @ tools/tool_backend_helpers.py:fal_key_is_configured */

/* Port of Python tools/tool_backend_helpers.py:fal_key_is_configured */
/* Return 1 when FAL_KEY is set to a non-whitespace value. */
int cli_tools_tool_backend_helpers_fal_key_is_configured(void)
{
    const char *value = getenv("FAL_KEY");
    if (!value || !*value) return 0;

    /* Check for whitespace-only value */
    const char *p = value;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (*p == '\0') return 0;

    return 1;
}



/* === dissolved from ports_wrappers.c === */
/* PoP: tools_tool_backend_helpers_managed_nous_tools_enabled @ tools/tool_backend_helpers.py:managed_nous_tools_enabled */
int tools_tool_backend_helpers_managed_nous_tools_enabled(const char *arg) {
    /* Python: entitlement gate. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "not_logged_in") == 0) { printf("0\n"); return 0; }
    if (strcmp(state, "error") == 0) { printf("0\n"); return 0; }
    printf("%s\n", (tab && tab[1] == '1') ? "1" : "0");
    return 0;
}

/* PoP: tools_tool_backend_helpers_normalize_browser_cloud_provider @ tools/tool_backend_helpers.py:normalize_browser_cloud_provider */
int tools_tool_backend_helpers_normalize_browser_cloud_provider(const char *arg) {
    /* Python: str(value or default).strip().lower() or default. */
    const char *v = arg ? arg : "";
    while (*v == ' ' || *v == '\t') v++;
    char buf[128];
    size_t n = strlen(v);
    if (n == 0) { printf("general-browser\n"); return 0; }
    if (n >= sizeof(buf)) n = sizeof(buf) - 1;
    memcpy(buf, v, n); buf[n] = '\0';
    for (char *p = buf; *p; p++) *p = (char)tolower((unsigned char)*p);
    printf("%s\n", buf[0] ? buf : "general-browser");
    return 0;
}

/* PoP: tools_tool_backend_helpers_coerce_modal_mode @ tools/tool_backend_helpers.py:coerce_modal_mode */
int tools_tool_backend_helpers_coerce_modal_mode(const char *arg) {
    /* Python: str(value or default).strip().lower(); default unless valid. */
    const char *v = (arg && *arg) ? arg : "";
    char buf[64];
    size_t n = strlen(v);
    if (n >= sizeof(buf)) n = sizeof(buf) - 1;
    memcpy(buf, v, n); buf[n] = '\0';
    for (char *p = buf; *p; p++) *p = (char)tolower((unsigned char)*p);
    /* valid modal modes: interactive, background, headless, observatory */
    if (strcmp(buf, "interactive") == 0 || strcmp(buf, "background") == 0 ||
        strcmp(buf, "headless") == 0 || strcmp(buf, "observatory") == 0) {
        printf("%s\n", buf);
        return 0;
    }
    printf("interactive\n");
    return 0;
}

/* PoP: tools_tool_backend_helpers_normalize_modal_mode @ tools/tool_backend_helpers.py:normalize_modal_mode */
int tools_tool_backend_helpers_normalize_modal_mode(const char *arg) {
    /* Python: coerce_modal_mode(value) — normalize modal execution mode. */
    if (!arg || !*arg) { printf("local\n"); return 0; }
    char low[16];
    size_t n = strlen(arg);
    if (n >= sizeof(low)) n = sizeof(low) - 1;
    for (size_t i = 0; i < n; i++) low[i] = (char)tolower((unsigned char)arg[i]);
    low[n] = '\0';
    if (strcmp(low, "1") == 0 || strcmp(low, "true") == 0 || strcmp(low, "yes") == 0 ||
        strcmp(low, "on") == 0 || strcmp(low, "modal") == 0) printf("modal\n");
    else if (strcmp(low, "0") == 0 || strcmp(low, "false") == 0 || strcmp(low, "no") == 0 ||
             strcmp(low, "off") == 0 || strcmp(low, "local") == 0) printf("local\n");
    else printf("%s\n", arg);
    return 0;
}

/* PoP: tools_tool_backend_helpers_has_direct_modal_credentials @ tools/tool_backend_helpers.py:has_direct_modal_credentials */
int tools_tool_backend_helpers_has_direct_modal_credentials(const char *arg) {
    /* Python: MODAL_TOKEN_ID+SECRET or ~/.modal.toml. Arg = "env_set\tmodal_file". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int env_set = (arg[0] == '1');
    int file_exists = tab ? (tab[1] == '1') : 0;
    printf("%d\n", (env_set || file_exists) ? 1 : 0);
    return 0;
}

/* PoP: tools_tool_backend_helpers_resolve_modal_backend_state @ tools/tool_backend_helpers.py:resolve_modal_backend_state */
int tools_tool_backend_helpers_resolve_modal_backend_state(const char *arg) {
    /* Python: direct/managed/auto matrix. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("{}\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("{}\n"); return 0; }
    printf("%s\n", tab ? tab + 1 : "{}");
    return 0;
}

/* PoP: tools_tool_backend_helpers_resolve_openai_audio_api_key @ tools/tool_backend_helpers.py:resolve_openai_audio_api_key */
int tools_tool_backend_helpers_resolve_openai_audio_api_key(const char *arg) {
    /* Python: (VOICE_TOOLS_OPENAI_KEY or OPENAI_API_KEY).strip(). */
    (void)arg;
    const char *v = getenv("VOICE_TOOLS_OPENAI_KEY");
    if (!v || !*v) v = getenv("OPENAI_API_KEY");
    if (!v) v = "";
    while (*v == ' ' || *v == '\t') v++;
    printf("%s\n", v);
    return 0;
}

/* PoP: tools_tool_backend_helpers__scoped_credential @ tools/tool_backend_helpers.py:_scoped_credential */
char *tools_tool_backend_helpers__scoped_credential(const char *name) {
    /* Python: read a credential env var under the active profile secret scope,
     * falling back to a raw getenv only when secret_scope is unavailable. */
    if (!name || !*name) return strdup("");
    extern char *secret_scope_get_secret(const char *name, const char *default_val);
    char *val = secret_scope_get_secret(name, "");
    if (!val) {
        /* Fallback: raw os.environ read (packaging edge — secret_scope unavailable) */
        const char *raw = getenv(name);
        return strdup(raw ? raw : "");
    }
    /* Python: .strip() */
    char *start = val;
    while (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r') start++;
    char *end = start + strlen(start);
    while (end > start && (*(end - 1) == ' ' || *(end - 1) == '\t' ||
                            *(end - 1) == '\n' || *(end - 1) == '\r')) {
        *--end = '\0';
    }
    if (start == val) return val;   /* nothing to trim — reuse */
    char *trimmed = strdup(start);
    free(val);
    return trimmed;
}

/* PoP: tools_tool_backend_helpers_resolve_provider_secret @ tools/tool_backend_helpers.py:resolve_provider_secret */
char *tools_tool_backend_helpers_resolve_provider_secret(
    const char *env_var, const char *provider_id, const char *config_value) {
    /* Python: resolve a voice/STT/TTS API key — single owner for key lookup.
     * Resolution order: config_value -> scoped env -> pool (non-multiplex only). */
    if (!env_var || !*env_var) return strdup("");

    /* 1. Explicit config value from config.yaml. */
    if (config_value && config_value[0]) {
        char *v = strdup(config_value);
        /* Python: str(...).strip() */
        char *s = v, *e = v + strlen(v);
        while (*s == ' ' || *s == '\t') s++;
        while (e > s && (*(e - 1) == ' ' || *(e - 1) == '\t')) *--e = '\0';
        char *trimmed = strdup(s);
        free(v);
        return trimmed;
    }

    /* 2. Scope-aware env read. */
    char *key = tools_tool_backend_helpers__scoped_credential(env_var);
    if (key && key[0]) return key;

    /* 3. Under multiplexing the profile scope is authoritative — do not fall
     *    through to the credential pool which may belong to another profile. */
    extern bool secret_scope_is_multiplex_active(void);
    if (secret_scope_is_multiplex_active()) {
        free(key);
        return strdup("");
    }

    /* 4. Credential pool — check both provider_id and custom:<provider_id>. */
    extern credential_pool_t *load_pool(const char *provider);
    if (provider_id && provider_id[0]) {
        char custom_key[1024];
        const char *keys[] = { provider_id, NULL };
        bool have_custom = snprintf(custom_key, sizeof(custom_key),
                                    "custom:%s", provider_id) < (int)sizeof(custom_key);
        if (have_custom) keys[1] = custom_key;

        for (int i = 0; i < (int)(sizeof(keys) / sizeof(keys[0])) && keys[i]; i++) {
            credential_pool_t *pool = load_pool(keys[i]);
            if (!pool || !credential_pool_has_available(pool)) {
                if (pool) credential_pool_free(pool);
                continue;
            }
            const char *k = credential_pool_next_key(pool, NULL);
            if (k && k[0]) {
                char *result = strdup(k);
                credential_pool_free(pool);
                free(key);
                return result;
            }
            credential_pool_free(pool);
        }
    }
    free(key);
    return strdup("");
}

/* PoP: tools_tool_backend_helpers_prefers_gateway @ tools/tool_backend_helpers.py:prefers_gateway */
int tools_tool_backend_helpers_prefers_gateway(const char *arg) {
    /* Python: config <section>.use_gateway truthy. Arg = "use_gateway\t1/0". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (tab && tab[1] == '1') { printf("1\n"); return 0; }
    if (strcmp(arg, "1") == 0 || strcasecmp(arg, "true") == 0 || strcasecmp(arg, "yes") == 0) { printf("1\n"); return 0; }
    printf("0\n");
    return 0;
}

/* === REAL_GAP closures: provider selection read/exists/error ============== */

#include "port_config_py_helpers.h" /* config_py_read_raw_config */
#include "truthy.h"                 /* is_truthy_value */

#define TBH_NOUS_MANAGED_PROVIDER "nous"

/* PoP: tools_tool_backend_helpers__str_or_none @ tools/tool_backend_helpers.py:read_selection._str_or_none */
/* Python inner helper _str_or_none: str(value).strip().lower() or None. */
static int tbh_str_or_none(json_t *raw, const char *key, char *out, size_t sz)
{
    out[0] = '\0';
    if (!raw || raw->type != JSON_OBJECT) return 0;
    json_t *v = json_obj_get(raw, key);
    if (!v) return 0;
    const char *s = NULL;
    if (v->type == JSON_STRING) s = json_get_str(v, "", "");
    else if (v->type == JSON_NUMBER) {
        snprintf(out, sz, "%g", v->num_val);
    }
    if (!s) return out[0] ? 1 : 0;
    size_t n = 0;
    while (*s == ' ' || *s == '\t') s++;
    for (; *s && n + 1 < sz; s++) out[n++] = (char)(*s >= 'A' && *s <= 'Z' ? *s + 32 : *s);
    /* right-trim */
    while (n > 0 && (out[n - 1] == ' ' || out[n - 1] == '\t')) n--;
    out[n] = '\0';
    return out[0] ? 1 : 0;
}

/* PoP: tools_tool_backend_helpers_read_selection @ tools/tool_backend_helpers.py:read_selection */
/* Return the stored `hermes tools` provider string for a config section.
 * Reads the RAW config.yaml so key presence means a selection was actually
 * written. Legacy shim: truthy use_gateway maps to "nous". Returns the name
 * in out ("nous" or vendor), or 0 when never configured (Python None). */
int tools_tool_backend_helpers_read_selection(const char *section,
                                              char *out, size_t sz)
{
    out[0] = '\0';
    if (!section || !*section || !out || sz == 0) return 0;
    json_t *cfg = config_py_read_raw_config();
    if (!cfg) return 0;
    json_t *raw = json_obj_get(cfg, section);
    char name[128] = "";
    if (raw && raw->type == JSON_OBJECT) {
        /* Per-section selection-name keys; browser.backend is deliberately
         * excluded (driver choice, not cloud-provider selection). */
        const char *keys[3];
        int nk = 0;
        if (strcmp(section, "browser") == 0) {
            keys[nk++] = "cloud_provider";
        } else if (strcmp(section, "web") == 0) {
            keys[nk++] = "backend";
        } else {
            keys[nk++] = "provider";
            keys[nk++] = "backend";
            keys[nk++] = "cloud_provider";
        }
        for (int i = 0; i < nk; i++) {
            if (tbh_str_or_none(raw, keys[i], name, sizeof(name))) break;
            name[0] = '\0';
        }
        /* Legacy shim: use_gateway:true was only ever written by the
         * managed "Nous Subscription" row. */
        json_t *ug = json_obj_get(raw, "use_gateway");
        if (ug) {
            bool truthy = false;
            if (ug->type == JSON_BOOL) truthy = ug->bool_val;
            else if (ug->type == JSON_STRING)
                truthy = is_truthy_value(json_get_str(ug, "", ""), false);
            if (truthy) {
                snprintf(out, sz, "%s", TBH_NOUS_MANAGED_PROVIDER);
                json_free(cfg);
                return 1;
            }
        }
    }
    json_free(cfg);
    if (name[0]) {
        snprintf(out, sz, "%s", name);
        return 1;
    }
    /* use_gateway:false with no name key is not a usable selection shape. */
    return 0;
}

/* PoP: tools_tool_backend_helpers_selection_exists @ tools/tool_backend_helpers.py:selection_exists */
/* True when ANY selection signal has ever been written for the section:
 * read_selection() hits, or per-capability web keys
 * (search_backend / extract_backend) are non-empty in raw config. */
int tools_tool_backend_helpers_selection_exists(const char *section)
{
    char sel[128];
    if (tools_tool_backend_helpers_read_selection(section, sel, sizeof(sel)))
        return 1;
    /* Extra per-capability keys only exist for the web section. */
    const char **extra = NULL;
    static const char *WEB_EXTRA[] = {"search_backend", "extract_backend", NULL};
    if (strcmp(section, "web") == 0) extra = WEB_EXTRA;
    if (!extra) return 0;
    json_t *cfg = config_py_read_raw_config();
    if (!cfg) return 0;
    json_t *raw = json_obj_get(cfg, section);
    bool any = false;
    if (raw && raw->type == JSON_OBJECT) {
        for (int i = 0; extra[i]; i++) {
            json_t *v = json_obj_get(raw, extra[i]);
            if (!v) continue;
            if (v->type == JSON_STRING && v->str_val && v->str_val[0] &&
                strspn(v->str_val, " \t") != strlen(v->str_val)) {
                any = true;
                break;
            }
            if (v->type == JSON_NUMBER) { any = true; break; }
        }
    }
    json_free(cfg);
    return any ? 1 : 0;
}

/* PoP: tools_tool_backend_helpers_selection_error @ tools/tool_backend_helpers.py:selection_error */
/* The uniform honest-error contract for a selected-but-broken provider. */
void tools_tool_backend_helpers_selection_error(const char *section,
                                                const char *selection_name,
                                                const char *failure,
                                                char *out, size_t sz)
{
    if (!out || sz == 0) return;
    snprintf(out, sz,
             "%s is configured to use %s (set via hermes tools), but %s. "
             "Run 'hermes tools' to change it.",
             section ? section : "",
             selection_name ? selection_name : "",
             failure ? failure : "");
}
