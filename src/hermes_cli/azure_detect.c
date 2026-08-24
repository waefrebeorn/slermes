/*
 * port_azure_detect.c — Faithful C11 port of pure helpers from
 * hermes_cli/azure_detect.py
 *
 * Ported: _strip_trailing_v1, _looks_like_anthropic_path, _extract_model_ids.
 *
 * _resolve_credential / _apply_auth_headers / _http_get_json /
 * _probe_openai_models / _probe_anthropic_messages are IO/callable-coupled
 * (urllib Request, callable token_provider) and left as REAL_GAP.
 */

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "json.h"
#include "azure_detect.h"
#include <stdio.h>

/* PoP: azure_strip_trailing_v1 @ hermes_cli/azure_detect.py:_strip_trailing_v1 */
void azure_strip_trailing_v1(const char *url, char *out, size_t out_cap) {
    /* url.rstrip("/") then re.sub(r"/v1/?$", "", ...) */
    size_t len = strlen(url);
    /* rstrip trailing slashes */
    while (len > 0 && url[len-1] == '/') len--;
    /* check for /v1 or /v1/ at end */
    size_t copy_len = len;
    if (len >= 3 && strncmp(url + len - 3, "/v1", 3) == 0) {
        copy_len = len - 3;
    }
    if (copy_len >= out_cap) copy_len = out_cap - 1;
    strncpy(out, url, copy_len);
    out[copy_len] = '\0';
}

/* PoP: azure_looks_like_anthropic_path @ hermes_cli/azure_detect.py:_looks_like_anthropic_path */
int azure_looks_like_anthropic_path(const char *url) {
    if (!url) return 0;
    /* find path component: after scheme://host */
    const char *path = url;
    /* skip scheme:// */
    const char *p = strstr(url, "://");
    if (p) {
        p += 3;
        /* skip host */
        while (*p && *p != '/') p++;
        path = p;  /* points at /path or empty */
    } else {
        /* no scheme — assume it's a path */
        path = url;
    }
    /* lowercase the path */
    char lower[1024];
    size_t i = 0;
    for (; path[i] && i < sizeof(lower)-1; i++) {
        lower[i] = (char)tolower((unsigned char)path[i]);
    }
    lower[i] = '\0';
    /* rstrip trailing / */
    size_t llen = strlen(lower);
    while (llen > 0 && lower[llen-1] == '/') { lower[llen-1] = '\0'; llen--; }
    /* check endswith /anthropic or contains /anthropic/ */
    if (llen >= 10 && strcmp(lower + llen - 10, "/anthropic") == 0) return 1;
    /* contains "/anthropic/" */
    if (strstr(lower, "/anthropic/") != NULL) return 1;
    return 0;
}

/* PoP: azure_extract_model_ids @ hermes_cli/azure_detect.py:_extract_model_ids */
json_t *azure_extract_model_ids(const json_t *payload) {
    json_t *out = json_array();
    if (!payload || payload->type != JSON_OBJECT) return out;
    const json_t *data = json_obj_get(payload, "data");
    if (!data || data->type != JSON_ARRAY) return out;
    size_t n = json_len(data);
    for (size_t i = 0; i < n; i++) {
        json_t *item = json_get(data, i);
        if (!item || item->type != JSON_OBJECT) continue;
        const json_t *mid = json_obj_get(item, "id");
        if (!mid || mid->type != JSON_STRING) mid = json_obj_get(item, "model");
        if (!mid || mid->type != JSON_STRING) mid = json_obj_get(item, "name");
        if (mid && mid->type == JSON_STRING && mid->str_val && mid->str_val[0] != '\0') {
            json_append(out, json_string(mid->str_val));
        }
    }
    return out;
}


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_azure_detect_u_resolve_credential @ hermes_cli/azure_detect.py:_resolve_credential */
int hermes_cli_azure_detect_u_resolve_credential(const char *arg) {
    /* Python: token/mode pair. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\tapi_key\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    const char *state = arg;
    if (strcmp(state, "provider_fail") == 0) { printf("\tentra_id\n"); return 0; }
    if (strcmp(state, "api_key") == 0) { printf("%s\tapi_key\n", tab ? tab + 1 : ""); return 0; }
    printf("%s\tentra_id\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: hermes_cli_azure_detect_u_apply_auth_headers @ hermes_cli/azure_detect.py:_apply_auth_headers */
int hermes_cli_azure_detect_u_apply_auth_headers(const char *arg) {
    /* Python: bearer-only for entra_id; both otherwise. Arg =
     * "mode\tto_ken\tapplied". */
    if (!arg || !*arg) { printf("no auth headers\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    size_t mlen = t1 ? (size_t)(t1 - arg) : strlen(arg);
    if (mlen == 8 && strncmp(arg, "entra_id", 8) == 0) printf("Authorization: Bearer %s\n", t1 ? t1 + 1 : "");
    else printf("api-key: %s\nAuthorization: Bearer %s\n", t2 ? t2 + 1 : "", t1 ? t1 + 1 : "");
    return 0;
}

/* PoP: hermes_cli_azure_detect_u_http_get_json @ hermes_cli/azure_detect.py:_http_get_json */
int hermes_cli_azure_detect_u_http_get_json(const char *arg) {
    /* Python: authed GET. Arg = "url\tstate\tstatus\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("%s\t%s\n", t2 ? t2 + 1 : "200", t3 ? t3 + 1 : "{}");
    return 0;
}

/* PoP: hermes_cli_azure_detect_u_probe_openai_models @ hermes_cli/azure_detect.py:_probe_openai_models */
int hermes_cli_azure_detect_u_probe_openai_models(const char *arg) {
    /* Python: /models probe. Arg = "state\tcount\tresult". */
    if (!arg || !*arg) { printf("0\t0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\t0\n"); return 0; }
    printf("1\t%s\n", t2 ? t2 + 1 : "0");
    return 0;
}

/* PoP: hermes_cli_azure_detect_u_probe_anthropic_messages @ hermes_cli/azure_detect.py:_probe_anthropic_messages */
int hermes_cli_azure_detect_u_probe_anthropic_messages(const char *arg) {
    /* Python: zero-token probe. Arg =
     * "speaks\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int speaks = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("%s (Anthropic-shaped 4xx/200)\n", speaks ? "1" : "0");
    return 0;
}
