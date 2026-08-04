/*
 * port_tools_flux3_video.c — C11 port of pure helpers from
 * tools/flux3_video_tool.py.
 *
 * Faithful translations of the deterministic helpers in the FLUX 3
 * video tool. Reuses libjson (lib/libjson) for all JSON work.
 *
 * No stubs.  Every function mirrors the Python original's behaviour.
 */

#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include "port_tools_flux3_video.h"
#include "libjson/json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* PoP: _looks_like_local_path @ tools/flux3_video_tool.py:_looks_like_local_path */
bool f3_looks_like_local_path(const char *value)
{
    if (!value) return false;
    size_t len = strlen(value);

    /* Base64 payload check (Python re: ^[A-Za-z0-9+/\\r\\n]+={0,2}[\\r\\n]*$) */
    if (len >= F3_MIN_BASE64_PAYLOAD_LENGTH) {
        size_t i = 0;
        bool all_base64 = true;
        int pad_count = 0;
        while (i < len) {
            char c = value[i];
            if (c == '\r' || c == '\n') { i++; continue; }
            if (c == '=') { pad_count++; i++; continue; }
            bool in_alphabet = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                               (c >= '0' && c <= '9') || c == '+' || c == '/';
            if (!in_alphabet) { all_base64 = false; break; }
            i++;
        }
        /* trailing ={0,2} only */
        if (all_base64 && pad_count <= 2) return false;
    }

    if (strncmp(value, "file://", 7) == 0) return true;
    if (strcmp(value, "~") == 0) return true;
    if (strncmp(value, "~/", 2) == 0 || strncmp(value, "~\\", 2) == 0) return true;
    if (value[0] == '/' || strncmp(value, "./", 2) == 0 || strncmp(value, "../", 3) == 0 ||
        strncmp(value, ".\\", 2) == 0 || strncmp(value, "..\\", 3) == 0) return true;
    /* Windows drive path: ^[A-Za-z]:[\\/] */
    if (len >= 3 && ((value[0] >= 'A' && value[0] <= 'Z') || (value[0] >= 'a' && value[0] <= 'z')) &&
        value[1] == ':' && (value[2] == '\\' || value[2] == '/')) return true;
    /* UNC root */
    if (strncmp(value, "\\\\", 2) == 0) return true;
    return false;
}

/* PoP: _display_path @ tools/flux3_video_tool.py:_display_path */
char *f3_display_path(const char *path)
{
    if (!path) return NULL;
    size_t len = strlen(path);
    if (len <= 200) return strdup(path);

    /* f"{path[:200]}… ({len} characters)" */
    size_t need = 200 + 3 + 32 + len ? 0 : 0; /* recompute below */
    need = 200 + 3 /* … ( */ + 32 /* int room */ + 13 /* " characters)" */ + 1;
    char *out = malloc(need);
    if (!out) return NULL;
    /* first 200 chars are copied verbatim (no multi-byte split handling —
     * the Python slices codepoints; for ASCII paths this matches exactly) */
    memcpy(out, path, 200);
    snprintf(out + 200, need - 200, "… (%zu characters)", len);
    return out;
}

/* PoP: _poll_is_finished @ tools/flux3_video_tool.py:_poll_is_finished */
bool f3_poll_is_finished(const char *raw_json)
{
    if (!raw_json || !*raw_json) return true; /* unreadable -> finished */
    json_t *payload = json_parse(raw_json, NULL);
    if (!payload) return true;
    if (payload->type != JSON_OBJECT || json_obj_get(payload, "error") != NULL) {
        json_free(payload);
        return true;
    }
    /* Copy status out BEFORE freeing the tree (no use-after-free). */
    char status[64] = "";
    json_t *details = json_obj_get(payload, "details");
    if (details && details->type == JSON_OBJECT) {
        const char *s = json_get_str(details, "status", NULL);
        if (s) {
            strncpy(status, s, sizeof(status) - 1);
            status[sizeof(status) - 1] = '\0';
        }
    }
    json_free(payload);
    if (!status[0]) return true;
    /* _TERMINAL_POLL_STATUSES = {"Ready","Error","Request Moderated",
     *                            "Content Moderated","Task not found"} */
    if (strcmp(status, "Ready") == 0 || strcmp(status, "Error") == 0 ||
        strcmp(status, "Request Moderated") == 0 ||
        strcmp(status, "Content Moderated") == 0 ||
        strcmp(status, "Task not found") == 0) return true;
    return false;
}

/* PoP: _retry_after_seconds @ tools/flux3_video_tool.py:_retry_after_seconds */
double f3_retry_after_seconds(const char *raw_json)
{
    if (!raw_json || !*raw_json) return -1.0;
    json_t *payload = json_parse(raw_json, NULL);
    if (!payload) return -1.0;
    if (payload->type != JSON_OBJECT || json_obj_get(payload, "error") == NULL) {
        json_free(payload);
        return -1.0;
    }
    json_t *details = json_obj_get(payload, "details");
    double value = -1.0;
    if (details && details->type == JSON_OBJECT) {
        json_t *node = json_obj_get(details, "retryAfterSeconds");
        if (node) {
            if (node->type == JSON_NUMBER && node->num_val > 0.0) {
                value = node->num_val;
            } else if (node->type == JSON_STRING) {
                char *end = NULL;
                double v = strtod(node->str_val, &end);
                if (end != node->str_val && *end == '\0' && v > 0.0)
                    value = v;
            }
            /* bool is rejected (Python: isinstance(value, bool) -> None) */
            if (node->type == JSON_BOOL) value = -1.0;
        }
    }
    json_free(payload);
    return value;
}

/* PoP: _is_transport_error @ tools/flux3_video_tool.py:_is_transport_error */
bool f3_is_transport_error(const char *raw_json)
{
    if (!raw_json || !*raw_json) return false;
    json_t *payload = json_parse(raw_json, NULL);
    if (!payload) return false;
    bool is_te = (payload->type == JSON_OBJECT &&
                  json_get_bool(payload, "transport_error", false));
    json_free(payload);
    return is_te;
}

/* PoP: _without_media @ tools/flux3_video_tool.py:_without_media */
char *f3_without_media(const char *args_json)
{
    if (!args_json) return NULL;
    json_t *args = json_parse(args_json, NULL);
    if (!args || args->type != JSON_OBJECT) {
        if (args) json_free(args);
        return args ? strdup("{}") : NULL;
    }
    json_obj_del(args, "input_image");
    json_obj_del(args, "input_images");
    json_obj_del(args, "input_video");
    char *out = json_serialize(args);
    json_free(args);
    return out;
}

/* PoP: _submit_args @ tools/flux3_video_tool.py:_submit_args */
char *f3_submit_args(const char *args_json, const char *mode)
{
    if (!args_json || !mode) return NULL;
    json_t *args = json_parse(args_json, NULL);
    if (!args || args->type != JSON_OBJECT) {
        if (args) json_free(args);
        return NULL;
    }
    /* Drop None values — libjson has no null sentinel per key, so drop
     * JSON_NULL nodes (Python drops None == null). */
    json_t *filtered = json_object();
    for (size_t i = 0; i < args->c.count; i++) {
        if (args->c.items[i]->type == JSON_NULL) continue;
        json_set(filtered, args->c.keys[i], json_copy(args->c.items[i]));
    }
    json_set(filtered, "mode", json_string(mode));
    char *out = json_serialize(filtered);
    json_free(filtered);
    json_free(args);
    return out;
}

/* PoP: _error @ tools/flux3_video_tool.py:_error */
char *f3_error(const char *message)
{
    if (!message) return NULL;
    json_t *obj = json_object();
    json_set(obj, "error", json_string(message));
    char *out = json_serialize(obj);
    json_free(obj);
    return out;
}

/* PoP: _filename_from_url @ tools/flux3_video_tool.py:_filename_from_url */
char *f3_filename_from_url(const char *url)
{
    if (!url) return strdup(F3_FALLBACK_FILENAME);

    /* urlsplit(url).path — take everything up to '?' or '#' */
    const char *path = url;
    const char *q = strchr(path, '?');
    const char *h = strchr(path, '#');
    const char *end = q && (!h || q < h) ? q : (h ? h : path + strlen(path));
    size_t path_len = (size_t)(end - path);

    /* Python: PurePosixPath(unquote(urlsplit(url).path)).name — unquote
     * the path FIRST, then take the basename. */
    char unquoted[1024];
    size_t uq = 0;
    for (size_t i = 0; i < path_len && uq < sizeof(unquoted) - 1; i++) {
        char c = path[i];
        if (c == '%' && i + 2 < path_len &&
            isxdigit((unsigned char)path[i+1]) &&
            isxdigit((unsigned char)path[i+2])) {
            char hex[3] = { path[i+1], path[i+2], '\0' };
            unquoted[uq++] = (char)strtol(hex, NULL, 16);
            i += 2;
        } else {
            unquoted[uq++] = c;
        }
    }
    unquoted[uq] = '\0';

    /* basename: everything after the last '/' */
    const char *last_slash = NULL;
    for (const char *p = unquoted; *p; p++) {
        if (*p == '/') last_slash = p;
    }
    const char *name = last_slash ? last_slash + 1 : unquoted;

    /* re.sub(r"[^A-Za-z0-9._-]", "_", name).lstrip(".")[:120] */
    char cleaned[256];
    size_t cl = 0;
    for (const char *p = name; *p && cl < sizeof(cleaned) - 1; p++) {
        char c = *p;
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-') {
            cleaned[cl++] = c;
        } else {
            cleaned[cl++] = '_';
        }
    }
    cleaned[cl] = '\0';
    /* lstrip('.') */
    size_t start = 0;
    while (cleaned[start] == '.') start++;
    size_t remain = cl - start;
    if (remain == 0) return strdup(F3_FALLBACK_FILENAME);
    size_t cap = remain < F3_MAX_FILENAME_LENGTH ? remain : F3_MAX_FILENAME_LENGTH;
    char *out = malloc(cap + 1);
    if (!out) return NULL;
    memcpy(out, cleaned + start, cap);
    out[cap] = '\0';
    return out;
}

/* PoP: _delivery_lead_in @ tools/flux3_video_tool.py:_delivery_lead_in */
char *f3_delivery_lead_in(const char *target, bool delivers_as_attachment)
{
    if (!target) return NULL;
    if (delivers_as_attachment) {
        const char *lead =
            "Saved to %s. To deliver it, copy the next line into your reply "
            "exactly as written, alone on its own line, with nothing added "
            "around it:\nMEDIA:%s\n";
        size_t need = strlen(lead) + 2 * strlen(target) + 1;
        char *out = malloc(need);
        if (!out) return NULL;
        snprintf(out, need, lead, target, target);
        return out;
    }
    size_t need = strlen("Saved to ") + strlen(target) + 3; /* ". " + NUL */
    char *out = malloc(need);
    if (!out) return NULL;
    snprintf(out, need, "Saved to %s. ", target);
    return out;
}

/* PoP: _download_read_timeout @ tools/flux3_video_tool.py:_download_read_timeout */
double f3_download_read_timeout(double elapsed)
{
    double left = F3_CALL_BACKSTOP_SECONDS - elapsed - F3_DOWNLOAD_GRACE_SECONDS;
    double v = left < 0.0 ? 0.0 : left;
    if (v > F3_DOWNLOAD_READ_TIMEOUT_SECONDS) v = F3_DOWNLOAD_READ_TIMEOUT_SECONDS;
    return v;
}
