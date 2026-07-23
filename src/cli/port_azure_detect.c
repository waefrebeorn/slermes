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
