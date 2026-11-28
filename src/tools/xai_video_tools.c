/*
 * xai_video_tools.c — canonical C11 port of tools/xai_video_tools.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include "hermes_core_types.h"
#include "registry.h"
#include "libhttp/http.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: tools_xai_video_tools_u_configured_for_xai_video @ tools/xai_video_tools.py:_configured_for_xai_video */
int tools_xai_video_tools_u_configured_for_xai_video(const char *arg) {
    /* Python: load_config().get("video_gen") is dict and provider == "xai".
     * Arg = "video_gen" section JSON (or empty). */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    json_t *cfg = json_parse(arg, NULL);
    if (!cfg || !json_is_object(cfg)) {
        if (cfg) json_free(cfg);
        printf("0\n");
        return 0;
    }
    const char *provider = json_get_str(cfg, "provider", "");
    printf("%d\n", strcmp(provider, "xai") == 0);
    json_free(cfg);
    return 0;
}

/* PoP: tools_xai_video_tools_u_check_xai_video_requirements @ tools/xai_video_tools.py:_check_xai_video_requirements */
int tools_xai_video_tools_u_check_xai_video_requirements(const char *arg) {
    /* Python: configured_for_xai_video() and has_xai_video_credentials().
     * Arg = "1/0 configured"; credentials from the env. */
    int configured = (arg && *arg && atoi(arg)) ? 1 : 0;
    const char *key = getenv("XAI_API_KEY");
    int has_creds = key && *key;
    return configured && has_creds;
}

/* PoP: tools_xai_video_tools_u_clean_string @ tools/xai_video_tools.py:_clean_string */
/* PoP: tools_xai_video_tools_u_clean_string @ tools/xai_video_tools.py:_clean_string */
/* Faithful: value.strip() if isinstance str and non-empty after strip else None.
 * Was: stripped only space/tab (python .strip() removes \n\r\f\v too) and
 * printed "null"/value with return 0. Returns owned string or NULL. */
char *tools_xai_video_tools_u_clean_string(const char *value) {
    const char *s = value ? value : "";
    while (*s==' '||*s=='\t'||*s=='\n'||*s=='\r'||*s=='\f'||*s=='\v') s++;
    char *d = strdup(s);
    size_t n = strlen(d);
    while (n > 0) { char c=d[n-1]; if (c==' '||c=='\t'||c=='\n'||c=='\r'||c=='\f'||c=='\v') d[--n]='\0'; else break; }
    if (n == 0) { free(d); return NULL; }
    return d;
}

/* PoP: tools_xai_video_tools_u_provider_not_configured_error @ tools/xai_video_tools.py:_provider_not_configured_error */
int tools_xai_video_tools_u_provider_not_configured_error(const char *arg) {
    /* Python: returns JSON error dict — REAL. */
    (void)arg;
    json_node_t *o = json_object();
    json_set(o, "success", json_bool(false));
    json_set(o, "error", json_string(
        "xAI video edit/extend tools require `video_gen.provider` to be "
        "configured as `xai` via `hermes tools` -> Video Generation."));
    json_set(o, "error_type", json_string("provider_not_configured"));
    json_set(o, "provider", json_string("xai"));
    char *s = json_serialize(o);
    json_free(o);
    printf("%s\n", s ? s : "{}");
    free(s);
    return 0;
}

/* PoP: tools_xai_video_tools_u_normalize_public_video_url @ tools/xai_video_tools.py:_normalize_public_video_url */
int tools_xai_video_tools_u_normalize_public_video_url(const char *arg) {
    /* Python: None if empty; keep only http(s):// URLs. Arg = url. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    while (*arg == ' ' || *arg == '\t') arg++;
    if (!*arg) { printf("\n"); return 0; }
    if (strncasecmp(arg, "http://", 7) == 0 || strncasecmp(arg, "https://", 8) == 0) {
        printf("%s\n", arg);
        return 0;
    }
    printf("\n");
    return 0;
}

/* PoP: tools_xai_video_tools_u_handle_xai_video_edit @ tools/xai_video_tools.py:_handle_xai_video_edit */
int tools_xai_video_tools_u_handle_xai_video_edit(const char *arg) {
    /* Python: prompt/url validation + run. Arg =
     * "prompt\tvideo_url\tconfigured\tresult". */
    if (!arg || !*arg) { printf("error: prompt is required for xAI video edit\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *prompt = arg;
    const char *url = t1 ? t1 + 1 : "";
    if (!prompt[0]) { printf("error: prompt is required for xAI video edit\n"); return 0; }
    if (!url[0]) {
        printf("error: video_url must be a public HTTPS MP4 URL (the `video`/`public_url` from a prior Imagine result)\n");
        return 0;
    }
    int configured = t2 && t2[1] == '1';
    if (!configured) { printf("error: provider not configured\n"); return 0; }
    const char *result = t3 ? t3 + 1 : "{}";
    printf("%s\n", result);
    return 0;
}

/* PoP: tools_xai_video_tools_u_handle_xai_video_extend @ tools/xai_video_tools.py:_handle_xai_video_extend */
int tools_xai_video_tools_u_handle_xai_video_extend(const char *arg) {
    /* Python: prompt/url gates + run. Arg =
     * "prompt\tvideo_url\tconfigured\tresult". */
    if (!arg || !*arg) { printf("error: prompt is required for xAI video extend\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *prompt = arg;
    const char *url = t1 ? t1 + 1 : "";
    if (!prompt[0]) { printf("error: prompt is required for xAI video extend\n"); return 0; }
    if (!url[0]) {
        printf("error: video_url must be a public HTTPS MP4 URL (the `video`/`public_url` from a prior Imagine result)\n");
        return 0;
    }
    int configured = t2 && t2[1] == '1';
    if (!configured) { printf("error: provider not configured\n"); return 0; }
    const char *result = t3 ? t3 + 1 : "{}";
    printf("%s\n", result);
    return 0;
}


/* ==================================================================
 *  Real tool handlers: xai_video_edit / xai_video_extend
 *
 *  The shims above take pre-computed inputs; these are the agent-facing
 *  handlers that validate, gate on the provider setting and credentials, and
 *  then drive the xAI OpenAI-compatible video endpoints
 *  (POST /v1/videos/{edits,extensions}, poll GET /v1/videos/{id}).
 *  Check order is upstream's contract: prompt, then video_url, then provider,
 *  then credentials -- nothing is sent to the network before all four pass.
 * ================================================================== */

#define XVT_DEFAULT_BASE  "https://api.x.ai/v1"
#define XVT_DEFAULT_MODEL "grok-imagine-video"
#define XVT_POLL_ATTEMPTS 60
#define XVT_POLL_SLEEP_MS 2000

static const char *XVT_AUTH_REQUIRED =
    "No xAI credentials found. Sign in via `hermes auth add xai-oauth` "
    "(SuperGrok / Premium+) or set XAI_API_KEY from https://console.x.ai/.";

static char *xvt_err(const char *msg, const char *type, const char *prompt) {
    json_t *o = json_object();
    json_set(o, "success", json_bool(false));
    json_set(o, "error", json_string(msg ? msg : "error"));
    if (type) json_set(o, "error_type", json_string(type));
    if (prompt) json_set(o, "prompt", json_string(prompt));
    char *t = json_serialize(o);
    json_free(o);
    return t;
}

/* "video_gen:" -> "provider:" out of the profile's config.yaml, by text (no yaml dependency). */
static char *xvt_video_gen_provider(void) {
    char home[1024], path[1200];
    hermes_get_home(home, sizeof home);
    if (!home[0]) return NULL;
    snprintf(path, sizeof path, "%s/config.yaml", home);

    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    static char buf[262144];
    size_t n = fread(buf, 1, sizeof buf - 1, f);
    buf[n] = '\0';
    fclose(f);

    const char *sec = strstr(buf, "video_gen:");
    if (!sec) return NULL;
    const char *p = strchr(sec, '\n');
    while (p && *p) {
        const char *line = p + 1;
        if (*line != ' ' && *line != '\t' && *line != '#') break;   /* next top-level key */
        const char *k = line;
        while (*k == ' ' || *k == '\t') k++;
        if (strncmp(k, "provider:", 9) == 0) {
            const char *v = k + 9;
            while (*v == ' ' || *v == '\t' || *v == '"' || *v == '\'') v++;
            const char *e = v;
            while (*e && *e != '\n' && *e != '\r' && *e != ' ' && *e != '"' && *e != '\'') e++;
            if (e == v) return NULL;
            char *out = malloc((size_t)(e - v) + 1);
            if (!out) return NULL;
            memcpy(out, v, (size_t)(e - v));
            out[e - v] = '\0';
            return out;
        }
        p = strchr(line, '\n');
    }
    return NULL;
}

static bool xvt_provider_is_xai(void) {
    const char *env = getenv("HERMES_VIDEO_GEN_PROVIDER");
    if (env && *env) return strcasecmp(env, "xai") == 0;
    char *v = xvt_video_gen_provider();
    bool ok = v && strcasecmp(v, "xai") == 0;
    free(v);
    return ok;
}

static void xvt_sleep_ms(int ms) {
#ifdef _WIN32
    Sleep((DWORD)ms);
#else
    usleep((useconds_t)ms * 1000);
#endif
}

static char *xvt_submit_and_poll(const char *api_key, const char *base, const char *endpoint,
                                 const char *model, const char *prompt, const char *video_url,
                                 int duration, bool with_duration) {
    json_t *payload = json_object();
    json_set(payload, "model", json_string(model));
    json_set(payload, "prompt", json_string(prompt));
    json_set(payload, "video", json_string(video_url));
    if (with_duration && duration > 0) json_set(payload, "duration", json_number((double)duration));
    char *body = json_serialize(payload);
    json_free(payload);
    if (!body) return xvt_err("could not build the request body", "api_error", prompt);

    char url[1024], auth[600];
    snprintf(url, sizeof url, "%s/videos/%s", base, endpoint);
    snprintf(auth, sizeof auth, "Authorization: Bearer %s\r\naccept: application/json", api_key);

    http_t *h = http_new(120);
    if (!h) { free(body); return xvt_err("cannot create the HTTP client", "api_error", prompt); }
    http_resp_t *resp = http_request(h, HTTP_POST, url, auth, body, strlen(body));
    free(body);
    if (!resp || resp->status < 200 || resp->status >= 300) {
        int st = resp ? resp->status : -1;
        char detail[220] = "";
        if (resp && resp->body) snprintf(detail, sizeof detail, "%.*s", 200, resp->body);
        if (resp) http_resp_free(resp);
        http_free(h);
        char msg[512];
        snprintf(msg, sizeof msg, "xAI rejected the request (HTTP %d)%s%s", st,
                 detail[0] ? ": " : "", detail);
        return xvt_err(msg, "api_error", prompt);
    }

    json_t *root = json_parse(resp->body ? resp->body : "", NULL);
    http_resp_free(resp);
    http_free(h);
    if (!root) return xvt_err("xAI returned a response that is not JSON", "api_error", prompt);

    const char *id = json_get_str(root, "id", NULL);
    char job_id[128] = "";
    if (id) snprintf(job_id, sizeof job_id, "%s", id);
    const char *status = json_get_str(root, "status", "");

    if (strcmp(status, "completed") != 0 && strcmp(status, "failed") != 0 && job_id[0]) {
        json_free(root);
        root = NULL;
        char get_url[1200], auth2[600];
        snprintf(get_url, sizeof get_url, "%s/videos/%s", base, job_id);
        snprintf(auth2, sizeof auth2, "Authorization: Bearer %s\r\naccept: application/json", api_key);
        for (int attempt = 0; attempt < XVT_POLL_ATTEMPTS && !root; attempt++) {
            xvt_sleep_ms(XVT_POLL_SLEEP_MS);
            http_t *h2 = http_new(60);
            if (!h2) break;
            http_resp_t *r2 = http_get(h2, get_url, auth2);
            if (r2 && r2->status >= 200 && r2->status < 300) {
                json_t *j2 = json_parse(r2->body ? r2->body : "", NULL);
                if (j2) {
                    const char *st2 = json_get_str(j2, "status", "");
                    if (strcmp(st2, "completed") == 0 || strcmp(st2, "failed") == 0 ||
                        strcmp(st2, "cancelled") == 0) root = j2;
                    else json_free(j2);
                }
            }
            if (r2) http_resp_free(r2);
            http_free(h2);
        }
        if (!root) {
            char msg[256];
            snprintf(msg, sizeof msg,
                     "xAI accepted the job (%s) but it did not finish within %d seconds",
                     job_id, (XVT_POLL_ATTEMPTS * XVT_POLL_SLEEP_MS) / 1000);
            json_t *o = json_object();
            json_set(o, "success", json_bool(false));
            json_set(o, "error", json_string(msg));
            json_set(o, "error_type", json_string("timeout"));
            json_set(o, "request_id", json_string(job_id));
            char *t = json_serialize(o);
            json_free(o);
            return t;
        }
    }

    json_t *out = json_object();
    json_set(out, "success", json_bool(true));
    json_set(out, "request_id", json_string(job_id));
    json_set(out, "operation", json_string(endpoint));
    json_set(out, "model", json_string(model));
    const char *st = json_get_str(root, "status", "");
    if (st && *st) json_set(out, "status", json_string(st));
    const char *video = json_get_str(root, "video", NULL);
    if (!video) video = json_get_str(root, "url", NULL);
    if (!video) {
        const json_t *data = json_obj_get(root, "data");
        if (data && data->type == JSON_ARRAY && json_len((json_t *)data) > 0) {
            const json_t *d0 = json_get((json_t *)data, 0);
            if (d0) video = json_get_str(d0, "url", NULL);
        }
    }
    if (video && *video) json_set(out, "video", json_string(video));
    json_free(root);
    char *t = json_serialize(out);
    json_free(out);
    return t;
}

static char *xvt_run(const char *args_json, const char *op, const char *endpoint, bool with_duration) {
    json_t *args = json_parse(args_json ? args_json : "{}", NULL);
    const char *prompt = args ? json_get_str(args, "prompt", "") : "";
    const char *video_url = args ? json_get_str(args, "video_url", "") : "";
    const json_t *dur = args ? json_obj_get(args, "duration") : NULL;
    int duration = (dur && dur->type == JSON_NUMBER) ? (int)dur->num_val : 0;

    if (!prompt || !*prompt) {
        char msg[96];
        snprintf(msg, sizeof msg, "prompt is required for xAI video %s", op);
        json_free(args);
        return xvt_err(msg, "missing_prompt", "");
    }
    if (!video_url || (strncasecmp(video_url, "http://", 7) != 0 &&
                       strncasecmp(video_url, "https://", 8) != 0)) {
        json_free(args);
        return xvt_err("video_url must be a public HTTPS MP4 URL (the `video`/`public_url` "
                       "from a prior Imagine result)", "missing_video", prompt);
    }
    if (!xvt_provider_is_xai()) {
        json_free(args);
        json_t *o = json_object();
        json_set(o, "success", json_bool(false));
        json_set(o, "error", json_string("xAI video edit/extend tools require `video_gen.provider` "
                                         "to be configured as `xai` via `hermes tools` -> Video Generation."));
        json_set(o, "error_type", json_string("provider_not_configured"));
        json_set(o, "provider", json_string("xai"));
        char *t = json_serialize(o);
        json_free(o);
        return t;
    }

    const char *api_key = getenv("XAI_API_KEY");
    if (!api_key || !*api_key) {
        json_free(args);
        return xvt_err(XVT_AUTH_REQUIRED, "auth_required", prompt);
    }
    const char *base = getenv("XAI_BASE_URL");
    if (!base || !*base) base = XVT_DEFAULT_BASE;
    const char *model = getenv("XAI_VIDEO_MODEL");
    if (!model || !*model) model = XVT_DEFAULT_MODEL;

    char *out = xvt_submit_and_poll(api_key, base, endpoint, model, prompt, video_url,
                                    duration, with_duration);
    json_free(args);
    return out;
}

static char *xvt_handle_edit(const char *a, const char *t)   { (void)t; return xvt_run(a, "edit", "edits", false); }
static char *xvt_handle_extend(const char *a, const char *t) { (void)t; return xvt_run(a, "extend", "extensions", true); }

void registry_init_xai_video_tools(void) {
    registry_register("xai_video_edit",
        "Edit an existing video with xAI Imagine. `video_url` must be the public HTTPS MP4 URL from a prior Imagine result (`video` or `public_url` on files-cdn).",
        "{\"type\":\"object\",\"properties\":{"
          "\"prompt\":{\"type\":\"string\",\"description\":\"Instruction for how xAI should modify the source video.\"},"
          "\"video_url\":{\"type\":\"string\",\"description\":\"Public HTTPS MP4 URL of the source video.\"}},"
          "\"required\":[\"prompt\",\"video_url\"]}",
        xvt_handle_edit);
    registry_register("xai_video_extend",
        "Extend an existing video with xAI Imagine. `video_url` must be the public HTTPS MP4 URL from a prior Imagine result.",
        "{\"type\":\"object\",\"properties\":{"
          "\"prompt\":{\"type\":\"string\",\"description\":\"Instruction for how xAI should continue the source video.\"},"
          "\"video_url\":{\"type\":\"string\",\"description\":\"Public HTTPS MP4 URL of the source video.\"},"
          "\"duration\":{\"type\":\"integer\",\"description\":\"Desired extension duration in seconds. xAI clamps this to its supported range.\"}},"
          "\"required\":[\"prompt\",\"video_url\"]}",
        xvt_handle_extend);
}
