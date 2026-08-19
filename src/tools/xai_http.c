/*
 * xai_http.c — xAI HTTP tool for Hermes C.
 * Exposes xAI API credential resolution and URL building as a tool.
 * Wraps libxai_http functions for agent-accessible credential/URL queries.
 */

/* PoP: xAI HTTP client (port of tools/xai_http) */

#include "hermes_core_types.h"
#include "hermes_agent.h"
#include "hermes_json.h"
#include "xai_http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *SCHEMA = "{"
    "\"type\":\"object\","
    "\"properties\":{"
      "\"action\":{\"type\":\"string\",\"description\":\"Action: check_credentials (default), get_api_key, get_base_url\",\"default\":\"check_credentials\"}"
    "},"
    "\"required\":[]"
"}";

static char *xai_http_handler(const char *args_json, const char *task_id) {
    (void)task_id;
    if (!args_json) return strdup("{\"error\":\"No args\"}");

    char *err = NULL;
    json_node_t *args = json_parse(args_json, &err);
    if (!args) { free(err); return strdup("{\"error\":\"JSON parse\"}"); }

    const char *action = json_object_get_string(args, "action", "check_credentials");

    json_node_t *result = json_new_object();

    if (strcmp(action, "check_credentials") == 0) {
        bool has_creds = has_xai_credentials();
        json_object_set(result, "has_credentials", json_new_bool(has_creds));
        if (has_creds) {
            char key_buf[256]; key_buf[0] = '\0';
            bool has_key = xai_get_api_key(key_buf);
            char url_buf[256];
            xai_get_base_url(url_buf);
            const char *key = has_key ? key_buf : NULL;
            const char *url = url_buf;
            json_object_set(result, "api_key_available", json_new_bool(key && key[0]));
            if (key) {
                /* Mask key for display */
                size_t klen = strlen(key);
                char masked[128];
                if (klen > 8) {
                    snprintf(masked, sizeof(masked), "%.*s...%s", 4, key, key + klen - 4);
                } else {
                    snprintf(masked, sizeof(masked), "****");
                }
                json_object_set(result, "api_key_masked", json_new_string(masked));
            }
            json_object_set(result, "base_url", json_new_string(url && url[0] ? url : "https://api.x.ai/v1"));
        }
        json_object_set(result, "user_agent", json_new_string(xai_user_agent()));
    } else if (strcmp(action, "get_api_key") == 0) {
        char key_buf2[256]; key_buf2[0] = '\0';
        bool has_key2 = xai_get_api_key(key_buf2);
        if (has_key2 && key_buf2[0]) {
            json_object_set(result, "api_key_available", json_new_bool(true));
        } else {
            json_object_set(result, "api_key_available", json_new_bool(false));
            json_object_set(result, "warning", json_new_string("xAI API key not configured"));
        }
    } else if (strcmp(action, "get_base_url") == 0) {
        char url_buf2[256];
        xai_get_base_url(url_buf2);
        json_object_set(result, "base_url", json_new_string(url_buf2[0] ? url_buf2 : XAI_DEFAULT_BASE_URL));
    } else {
        json_object_set(result, "error", json_new_string("Unknown action"));
    }

    json_free(args);
    char *json_out = json_serialize(result);
    json_free(result);
    return json_out;
}

void registry_init_xai_http(void) {
    registry_register("xai_http",
        "Query xAI API configuration: check credentials availability, "
        "get masked API key status, base URL, and user agent string. "
        "Actions: check_credentials, get_api_key, get_base_url.",
        SCHEMA, xai_http_handler);
}

/* === merged from port_tools_xai_http.c === */
/*
 * port_tools_xai_http.c — C port of tools/xai_http.c
 */

#include "hermes_logger.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

/* PoP: cli_tools_xai_http_hermes_xai_user_agent @ tools/xai_http.py:hermes_xai_user_agent */

/* Port of Python tools/xai_http.py:hermes_xai_user_agent */
/* Return a stable Hermes-specific User-Agent for xAI HTTP calls. */
char *cli_tools_xai_http_hermes_xai_user_agent(void)
{
    const char *ver = HERMES_VERSION;
    if (!ver) ver = "unknown";

    /* Allocate result: "Hermes-Agent/<version>" */
    size_t len = strlen("Hermes-Agent/") + strlen(ver) + 1;
    char *result = (char *)malloc(len);
    if (result) {
        snprintf(result, len, "Hermes-Agent/%s", ver);
    }
    return result ? result : strdup("Hermes-Agent/unknown");
}

/* PoP: cli_tools_xai_http_hermes_xai_default_headers @ tools/xai_http.py:hermes_xai_default_headers */
char *cli_tools_xai_http_hermes_xai_default_headers(void)
{
    /* Default headers for OpenAI-SDK and raw HTTP clients talking to xAI.
     * Replaces the OpenAI Python SDK's identifying User-Agent so chat/
     * completions and Responses traffic is attributed as Hermes Agent,
     * matching the direct HTTP integrations. Returns a malloc'd JSON
     * object string with a single "User-Agent" key. */
    char *ua = cli_tools_xai_http_hermes_xai_user_agent();
    if (!ua) ua = strdup("Hermes-Agent/unknown");
    /* Build {"User-Agent": "<ua>"} via the json_t builder. */
    json_t *obj = json_new_object();
    if (!obj) { free(ua); return NULL; }
    json_object_set(obj, "User-Agent", json_new_string(ua));
    char *out = json_dumps(obj, 0);
    json_free(obj);
    free(ua);
    return out ? out : strdup("{\"User-Agent\":\"Hermes-Agent/unknown\"}");
}

/* PoP: cli_tools_xai_http_resolve_xai_http_credentials @ tools/xai_http.py:resolve_xai_http_credentials */
/* Port of Python tools/xai_http.py:resolve_xai_http_credentials */
/* Resolve bearer credentials for direct xAI HTTP endpoints.
 * In the C port, we return a JSON string with provider/api_key/base_url.
 * Caller is responsible for freeing the returned string. */
char *cli_tools_xai_http_resolve_xai_http_credentials(int force_refresh)
{
    (void)force_refresh; /* OAuth refresh path is Python-specific */

    /* Try XAI_API_KEY from environment first */
    const char *api_key = getenv("XAI_API_KEY");
    const char *base_url = getenv("XAI_BASE_URL");

    if (!api_key || !api_key[0]) api_key = "";
    if (!base_url || !base_url[0]) base_url = "https://api.x.ai/v1";
    else {
        /* rstrip("/") */
        size_t len = strlen(base_url);
        char *trimmed = (char *)malloc(len + 1);
        if (trimmed) {
            strcpy(trimmed, base_url);
            while (len > 0 && trimmed[len - 1] == '/') {
                trimmed[--len] = '\0';
            }
            base_url = trimmed;
        }
    }

    /* Build result JSON: {"provider":"xai","api_key":"...","base_url":"..."} */
    size_t result_len = 64 + strlen(api_key) + strlen(base_url);
    char *result = (char *)malloc(result_len);
    if (result) {
        snprintf(result, result_len,
                 "{\"provider\":\"xai\",\"api_key\":\"%s\",\"base_url\":\"%s\"}",
                 api_key, base_url);
    }

    if (api_key && api_key[0]) {
        hermes_log(LOG_DEBUG, "port", "xai_http: resolved credentials from XAI_API_KEY env var");
    } else {
        hermes_log(LOG_DEBUG, "port", "xai_http: no credentials found, returning empty key");
    }

    return result ? result : strdup("{\"provider\":\"xai\",\"api_key\":\"\",\"base_url\":\"https://api.x.ai/v1\"}");
}

/* PoP: cli_tools_xai_http__coerce_expires_after @ tools/xai_http.py:_coerce_expires_after */

/* Port of Python tools/xai_http.py:_coerce_expires_after.
 * Normalize an xAI storage TTL: int seconds, or None for permanent.
 * Returns malloc'd string: the decimal seconds, or "null" for permanent. */
char *cli_tools_xai_http__coerce_expires_after(const char *value)
{
    /* MAX_XAI_STORAGE_EXPIRES_AFTER_SECONDS = 30*24*60*60 = 2592000
     * SAFE_XAI_STORAGE_EXPIRES_AFTER_SECONDS = 2*24*60*60 = 172800 */
    const long MAX_EXP = 30L * 24 * 60 * 60;   /* 2592000 */
    const long SAFE_EXP = 2L * 24 * 60 * 60;    /* 172800 */

    /* None -> None (permanent) */
    if (!value) {
        return strdup("null");
    }
    /* str(value) normalization; if value already numeric string, use it. */
    char buf[256];
    /* Convert the input to a normalized lowercase string for the keyword checks. */
    size_t j = 0;
    for (const char *s = value; *s && j + 1 < sizeof(buf); s++) {
        unsigned char c = (unsigned char)*s;
        buf[j++] = (char)tolower(c);
    }
    buf[j] = '\0';
    /* strip surrounding whitespace */
    char *b = buf;
    while (*b == ' ' || *b == '\t') b++;
    size_t L = strlen(b);
    while (L > 0 && (b[L - 1] == ' ' || b[L - 1] == '\t')) b[--L] = '\0';

    if (b[0] == '\0' || strcmp(b, "default") == 0
        || strcmp(b, "none") == 0 || strcmp(b, "null") == 0
        || strcmp(b, "never") == 0 || strcmp(b, "permanent") == 0
        || strcmp(b, "forever") == 0 || strcmp(b, "0") == 0) {
        return strdup("null");
    }

    /* Try int parse */
    char *endp = NULL;
    long seconds = strtol(b, &endp, 10);
    if (endp != b && (*endp == '\0' || *endp == ' ' || *endp == '\t')) {
        if (seconds <= 0) {
            return strdup("null");
        }
        if (seconds > MAX_EXP) seconds = MAX_EXP;
        char out[32];
        snprintf(out, sizeof(out), "%ld", seconds);
        return strdup(out);
    }

    /* Unparseable -> SAFE default */
    char out[32];
    snprintf(out, sizeof(out), "%ld", SAFE_EXP);
    return strdup(out);
}

/* ================================================================
 *  xAI Imagine storage config helpers
 * ================================================================ */

#include "hermes_yaml.h"

#include <sys/stat.h>

/* Resolve the config.yaml path: $HERMES_HOME/config.yaml or $HOME/config.yaml. */
static void xai_http_config_path(char *out, size_t out_size)
{
    const char *home = getenv("HERMES_HOME");
    if (!home || !home[0]) home = getenv("HOME");
    if (!home || !home[0]) home = "";
    snprintf(out, out_size, "%s/config.yaml", home);
}

/* PoP: cli_tools_xai_http__load_config_section @ tools/xai_http.py:_load_config_section */
/* Returns malloc'd JSON of the top-level config section, or "{}". */
char *cli_tools_xai_http__load_config_section(const char *section_name)
{
    char path[1024];
    xai_http_config_path(path, sizeof(path));
    char *err = NULL;
    yaml_doc_t *doc = yaml_parse_file(path, &err);
    if (err) free(err);
    if (!doc) return strdup("{}");
    char *json = yaml_to_json_string(doc, section_name ? section_name : "");
    yaml_free(doc);
    if (json) return json;
    /* section not present -> empty object */
    return strdup("{}");
}

/* PoP: cli_tools_xai_http_read_xai_imagine_storage_config @ tools/xai_http.py:read_xai_imagine_storage_config */
/* Reads storage settings under <section>.xai.storage. Returns malloc'd JSON
 * {"enabled","public_url","expires_after"(int-or-null)}. */
char *cli_tools_xai_http_read_xai_imagine_storage_config(const char *section_name)
{
    char path[1024];
    xai_http_config_path(path, sizeof(path));
    char *err = NULL;
    yaml_doc_t *doc = yaml_parse_file(path, &err);
    if (err) free(err);
    if (!doc) return strdup("{\"enabled\":true,\"public_url\":true,\"expires_after\":null}");

    char sp[256];
    snprintf(sp, sizeof(sp), "%s.xai.storage.enabled", section_name ? section_name : "image_gen");
    int enabled = yaml_get_bool(doc, sp, 1);
    snprintf(sp, sizeof(sp), "%s.xai.storage.public_url", section_name ? section_name : "image_gen");
    int public_url = yaml_get_bool(doc, sp, 1);
    snprintf(sp, sizeof(sp), "%s.xai.storage.expires_after", section_name ? section_name : "image_gen");
    const char *ea = yaml_get_string(doc, sp);

    char *coerced = cli_tools_xai_http__coerce_expires_after(ea);
    char buf[128];
    snprintf(buf, sizeof(buf),
             "{\"enabled\":%s,\"public_url\":%s,\"expires_after\":%s}",
             enabled ? "true" : "false",
             public_url ? "true" : "false",
             coerced ? coerced : "null");
    free(coerced);
    yaml_free(doc);
    return strdup(buf);
}

/* PoP: NULL @ tools/xai_http.py:build_xai_storage_options */
/* Returns malloc'd storage_options JSON, or NULL (strdup("null")) when disabled. */
char *cli_tools_xai_http_build_xai_storage_options(const char *section_name,
                                                   const char *filename_prefix,
                                                   const char *extension)
{
    char path[1024];
    xai_http_config_path(path, sizeof(path));
    char *err = NULL;
    yaml_doc_t *doc = yaml_parse_file(path, &err);
    if (err) free(err);
    if (!doc) return strdup("null");

    char sp[256];
    snprintf(sp, sizeof(sp), "%s.xai.storage.enabled", section_name ? section_name : "image_gen");
    int enabled = yaml_get_bool(doc, sp, 1);
    if (!enabled) { yaml_free(doc); return strdup("null"); }

    snprintf(sp, sizeof(sp), "%s.xai.storage.expires_after", section_name ? section_name : "image_gen");
    const char *ea = yaml_get_string(doc, sp);
    char *coerced = cli_tools_xai_http__coerce_expires_after(ea); /* "null" or decimal */

    time_t now = time(NULL);
    struct tm tmv;
#ifdef _WIN32
    gmtime_s(&tmv, &now);
#else
    gmtime_r(&now, &tmv);
#endif
    char ts[32];
    strftime(ts, sizeof(ts), "%Y%m%d-%H%M%S", &tmv);
    unsigned int r = (unsigned int)(now ^ (unsigned int)clock());
    char short8[16];
    snprintf(short8, sizeof(short8), "%08x", r);
    const char *ext = extension ? extension : "";
    while (ext[0] == '.') ext++;
    if (!ext[0]) ext = "bin";
    const char *fp = filename_prefix ? filename_prefix : "file";

    char fname[512];
    snprintf(fname, sizeof(fname), "%s-%s-%s.%s", fp, ts, short8, ext);

    char *out = malloc(256 + strlen(fname) + (coerced ? strlen(coerced) : 4));
    if (coerced && strcmp(coerced, "null") != 0)
        snprintf(out, 256 + strlen(fname) + strlen(coerced),
                 "{\"filename\":\"%s\",\"public_url\":true,\"expires_after\":%s}", fname, coerced);
    else
        snprintf(out, 256 + strlen(fname),
                 "{\"filename\":\"%s\",\"public_url\":true}", fname);
    free(coerced);
    yaml_free(doc);
    return out;
}

/* PoP: cli_tools_xai_http_xai_storage_notice_text @ tools/xai_http.py:xai_storage_notice_text */
char *cli_tools_xai_http_xai_storage_notice_text(const char *section_name)
{
    char path[1024];
    xai_http_config_path(path, sizeof(path));
    char *err = NULL;
    yaml_doc_t *doc = yaml_parse_file(path, &err);
    if (err) free(err);
    if (!doc) return strdup("");
    char sp[256];
    snprintf(sp, sizeof(sp), "%s.xai.storage.enabled", section_name ? section_name : "image_gen");
    int enabled = yaml_get_bool(doc, sp, 1);
    if (!enabled) { yaml_free(doc); return strdup(""); }
    snprintf(sp, sizeof(sp), "%s.xai.storage.expires_after", section_name ? section_name : "image_gen");
    const char *ea = yaml_get_string(doc, sp);
    char *coerced = cli_tools_xai_http__coerce_expires_after(ea);

    char *ret;
    if (coerced && strcmp(coerced, "null") != 0) {
        double secs = strtod(coerced, NULL);
        double days = secs / (24.0 * 60 * 60);
        char buf[512];
        snprintf(buf, sizeof(buf),
            "xAI Imagine storage is enabled so generated media gets a reusable "
            "public URL for about %.0g day%s. xAI may bill for stored files and public URL "
            "hosting. Disable this with `%s.xai.storage.enabled: false` "
            "or set `expires_after` to change the retention.",
            days, days != 1 ? "s" : "", section_name ? section_name : "image_gen");
        ret = strdup(buf);
    } else {
        size_t need = 256 + strlen(section_name ? section_name : "image_gen");
        char *buf = malloc(need);
        snprintf(buf, need,
            "xAI Imagine storage is enabled so generated media gets a reusable "
            "public URL without an automatic expiry. xAI may bill for stored files and public URL "
            "hosting. Disable this with `%s.xai.storage.enabled: false` "
            "or set `expires_after` to change the retention.",
            section_name ? section_name : "image_gen");
        ret = buf;
    }
    free(coerced);
    yaml_free(doc);
    return ret;
}

/* PoP: seen @ tools/xai_http.py:maybe_mark_xai_storage_notice_seen */
/* Returns the notice once per Hermes home, then marks it seen (state dir). */
char *cli_tools_xai_http_maybe_mark_xai_storage_notice_seen(const char *section_name)
{
    char *notice = cli_tools_xai_http_xai_storage_notice_text(section_name);
    if (!notice || !notice[0]) return NULL;
    const char *home = getenv("HERMES_HOME");
    if (!home || !home[0]) home = getenv("HOME");
    if (!home || !home[0]) return notice; /* no home -> always return notice */
    char marker[1024];
    snprintf(marker, sizeof(marker), "%s/state/%s_xai_storage_notice_seen",
             home, section_name ? section_name : "image_gen");
    /* ensure state dir exists */
    char statedir[1024];
    snprintf(statedir, sizeof(statedir), "%s/state", home);
    mkdir(statedir, 0700);
    /* already seen? */
    if (access(marker, F_OK) == 0) { free(notice); return NULL; }
    FILE *f = fopen(marker, "w");
    if (f) { fprintf(f, "seen\n"); fclose(f); }
    return notice;
}


