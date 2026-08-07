/*
 * flux3_video_tool.c — Pure helper ports from tools/flux3_video_tool.py.
 * HTTP-bound functions (_call_gateway, _download_video, _poll_until_done,
 * _submit, _handle_*) require async httpx transport and are ported separately.
 */
#define _GNU_SOURCE
#include "flux3_video_tool.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <sys/stat.h>

#define GW_URL_MAX 512

/* PoP: _looks_like_local_path @ tools/flux3_video_tool.py:_looks_like_local_path */
bool flux3_looks_like_local_path(const char *value)
{
    if (!value || !*value) return false;
    /* Check base64 payload FIRST (a JPEG base64 starts with "/9j/" → looks like path) */
    if (strlen(value) >= 256) {
        bool all_b64 = true;
        for (const char *p = value; *p; p++) {
            char c = *p;
            if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                  (c >= '0' && c <= '9') || c == '+' || c == '/' || c == '=' ||
                  c == '\r' || c == '\n')) {
                all_b64 = false;
                break;
            }
        }
        if (all_b64) return false;
    }
    if (strncmp(value, "file://", 7) == 0) return true;
    if (strcmp(value, "~") == 0) return true;
    if (strncmp(value, "~/", 2) == 0) return true;
    if (strncmp(value, "~\\", 2) == 0) return true;
    if (value[0] == '/' || strncmp(value, "./", 2) == 0 ||
        strncmp(value, "../", 3) == 0 || strncmp(value, ".\\", 2) == 0 ||
        strncmp(value, "..\\", 3) == 0)
        return true;
    /* Windows drive path: C:\ or C:/ */
    if (strlen(value) >= 3 && isalpha((unsigned char)value[0]) &&
        value[1] == ':' && (value[2] == '\\' || value[2] == '/'))
        return true;
    /* UNC path: \\server\share */
    if (value[0] == '\\' && value[1] == '\\') return true;
    return false;
}

/* PoP: _display_path @ tools/flux3_video_tool.py:_display_path */
char *flux3_display_path(const char *path)
{
    if (!path) return strdup("");
    size_t len = strlen(path);
    if (len <= 200) {
        char *out = (char *)malloc(len + 1);
        if (out) strcpy(out, path);
        return out;
    }
    /* Python: f"{path[:200]}… ({len(path)} characters)" */
    char *out = (char *)malloc(200 + 1 + 32);
    if (!out) return NULL;
    memcpy(out, path, 200);
    out[200] = 0xe2; /* UTF-8 ellipsis (U+2026 = E2 80 A6) */
    out[201] = 0x80;
    out[202] = 0xa6;
    out[203] = '\0';
    /* Append " (len characters)" — but we already wrote 200 chars; fix: */
    /* Actually redo: 200 chars + ellipsis + " (N characters)" */
    free(out);
    out = (char *)malloc(200 + 4 + 32);
    if (!out) return NULL;
    memcpy(out, path, 200);
    out[200] = 0xe2;
    out[201] = 0x80;
    out[202] = 0xa6;
    snprintf(out + 203, 32, " (%zu characters)", len);
    return out;
}

/* PoP: _filename_from_url @ tools/flux3_video_tool.py:_filename_from_url */
char *flux3_filename_from_url(const char *url)
{
    if (!url || !*url) return strdup("flux3-video.mp4");
    /* Extract path between the 3rd '/' and the next '/' or '?' or '#' */
    /* urlsplit(url).path — find scheme://host/path */
    const char *p = strstr(url, "://");
    const char *path_start = p ? p + 3 : url;
    /* find first '/' after host */
    const char *slash = strchr(path_start, '/');
    const char *seg = slash ? slash : path_start;
    /* find end of last path segment */
    const char *seg_end = seg;
    /* Walk forward to find the last segment */
    const char *last_slash = seg;
    for (const char *q = seg; *q; q++) {
        if (*q == '/' || *q == '?' || *q == '#') {
            last_slash = q;
        }
    }
    /* Actually: take everything from last_slash+1 up to ? or #, but simplest:
     * take the basename of the path */
    /* Find the end: next ? or # after path_start */
    const char *qend = strpbrk(path_start, "?#");
    size_t path_len = qend ? (size_t)(qend - path_start) : strlen(path_start);
    /* last '/' in the path */
    const char *last = seg;
    for (const char *q = seg; q < seg + path_len; q++) {
        if (*q == '/') last = q;
    }
    const char *name_start = last;
    if (*name_start == '/') name_start++;
    size_t name_len = (seg + path_len) - name_start;
    char raw[FLUX3_MAX_FILENAME_LEN + 1];
    size_t copy_len = name_len < sizeof(raw) ? name_len : sizeof(raw) - 1;
    /* URL-decode is omitted (Python uses unquote, but filenames here are simple) */
    memcpy(raw, name_start, copy_len);
    raw[copy_len] = '\0';
    /* Sanitize: keep only [A-Za-z0-9._-], replace others with _, strip leading dots, cap 120 */
    char clean[FLUX3_MAX_FILENAME_LEN + 1];
    size_t out_pos = 0;
    bool leading_dot = true;
    for (size_t i = 0; raw[i] && out_pos < FLUX3_MAX_FILENAME_LEN; i++) {
        char c = raw[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-') {
            if (leading_dot && c == '.') continue;
            leading_dot = false;
            clean[out_pos++] = c;
        } else {
            if (!leading_dot && out_pos < FLUX3_MAX_FILENAME_LEN) {
                clean[out_pos++] = '_';
            }
        }
    }
    clean[out_pos] = '\0';
    if (out_pos == 0) return strdup("flux3-video.mp4");
    return strdup(clean);
}

/* ── JSON helpers for polling ─────────────────────────── */

/* Simple: check if raw JSON string has "transport_error": true */
static bool gw_json_get_bool_field(const char *raw, const char *field)
{
    if (!raw || !field) return false;
    char needle[128];
    snprintf(needle, sizeof(needle), "\"%s\"", field);
    const char *p = strstr(raw, needle);
    if (!p) return false;
    p += strlen(needle);
    while (*p && (*p == ' ' || *p == ':' || *p == '\t')) p++;
    return strncmp(p, "true", 4) == 0;
}

/* PoP: _is_transport_error @ tools/flux3_video_tool.py:_is_transport_error */
bool flux3_is_transport_error(const char *raw)
{
    if (!raw || !*raw) return false;
    return gw_json_get_bool_field(raw, "transport_error");
}

/* Check if JSON has "error" key (any value) */
static bool gw_json_has_key(const char *raw, const char *key)
{
    if (!raw || !key) return false;
    char needle[128];
    snprintf(needle, sizeof(needle), "\"%s\"", key);
    return strstr(raw, needle) != NULL;
}

/* Extract status string from payload.details.status */
static bool gw_json_status_is_terminal(const char *raw)
{
    if (!raw) return false;
    /* Look for "status": "<value>" inside "details" */
    const char *details = strstr(raw, "\"details\"");
    if (!details) return false;
    const char *status = strstr(details, "\"status\"");
    if (!status) {
        /* No status field => not terminal per Python? Actually Python returns
         * True if status is not a string — meaning "no status = finished" */
        return true;
    }
    status += strlen("\"status\"");
    while (*status && (*status == ' ' || *status == ':' || *status == '\t')) status++;
    if (*status != '"') return true; /* not a string => terminal */
    const char *v = status + 1;
    char val[64];
    size_t i = 0;
    while (*v && *v != '"' && i < sizeof(val) - 1) {
        val[i++] = *v++;
    }
    val[i] = '\0';
    return strcmp(val, "Ready") == 0 ||
           strcmp(val, "Error") == 0 ||
           strcmp(val, "Request Moderated") == 0 ||
           strcmp(val, "Content Moderated") == 0 ||
           strcmp(val, "Task not found") == 0;
}

/* PoP: _poll_is_finished @ tools/flux3_video_tool.py:_poll_is_finished */
bool flux3_poll_is_finished(const char *raw)
{
    if (!raw || !*raw) return true;
    /* If it has "error" key, it's finished (refusal carries own guidance) */
    if (gw_json_has_key(raw, "error")) return true;
    /* Check terminal statuses */
    return gw_json_status_is_terminal(raw);
}

/* PoP: _retry_after_seconds @ tools/flux3_video_tool.py:_retry_after_seconds */
double flux3_retry_after_seconds(const char *raw)
{
    if (!raw || !*raw) return 0.0;
    if (!gw_json_has_key(raw, "error")) return 0.0;
    const char *details = strstr(raw, "\"details\"");
    if (!details) return 0.0;
    const char *retry = strstr(details, "\"retryAfterSeconds\"");
    if (!retry) return 0.0;
    retry += strlen("\"retryAfterSeconds\"");
    while (*retry && (*retry == ' ' || *retry == ':' || *retry == '\t')) retry++;
    /* Must be a number (not bool) */
    if (strncmp(retry, "true", 4) == 0 || strncmp(retry, "false", 5) == 0) return 0.0;
    char *end = NULL;
    double val = strtod(retry, &end);
    if (end == retry || val <= 0.0) return 0.0;
    return val;
}

/* PoP: _still_generating @ tools/flux3_video_tool.py:_still_generating */
char *flux3_still_generating(const char *job_id)
{
    char *out = NULL;
    if (!job_id) return NULL;
    /* Python uses em dash (U+2014, UTF-8 E2 80 94) not hyphen */
    asprintf(&out,
        "{\"result\": \"Still generating. This call reached its own time limit, "
        "which the job is unaffected by \xe2\x80\x94 call bfl_flux3_get_result again with "
        "id=%s to keep waiting.\", \"details\": {\"id\": \"%s\", \"status\": \"Generating\"}}",
        job_id, job_id);
    return out;
}

/* PoP: _without_media @ tools/flux3_video_tool.py:_without_media */
json_t *flux3_without_media(json_t *args)
{
    json_t *out = json_object();
    if (!args || args->type != JSON_OBJECT) return out;
    for (size_t i = 0; i < args->c.count; i++) {
        const char *k = args->c.keys[i];
        if (strcmp(k, "input_image") == 0 || strcmp(k, "input_images") == 0 ||
            strcmp(k, "input_video") == 0)
            continue;
        json_set(out, k, json_copy(args->c.items[i]));
    }
    return out;
}

/* PoP: _submit_args @ tools/flux3_video_tool.py:_submit_args */
json_t *flux3_submit_args(const char *mode, json_t *args)
{
    json_t *body = json_object();
    if (args && args->type == JSON_OBJECT) {
        for (size_t i = 0; i < args->c.count; i++) {
            const char *k = args->c.keys[i];
            if (args->c.items[i]->type == JSON_NULL) continue;
            json_set(body, k, json_copy(args->c.items[i]));
        }
    }
    json_set(body, "mode", json_string(mode ? mode : ""));
    return body;
}

/* PoP: _resolve_destination @ tools/flux3_video_tool.py:_resolve_destination */
char *flux3_resolve_destination(const char *save_to, const char *filename)
{
    if (!filename) filename = "flux3-video.mp4";
    char dir[4096];
    if (save_to && *save_to) {
        strncpy(dir, save_to, sizeof(dir) - 1);
        dir[sizeof(dir) - 1] = '\0';
        /* Strip trailing slashes */
        size_t len = strlen(dir);
        while (len > 0 && (dir[len-1] == '/' || dir[len-1] == '\\')) {
            dir[--len] = '\0';
        }
        struct stat st;
        if (stat(dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
            /* It's a file path, not a directory */
            /* Use the full save_to as the path */
            char *out = strdup(save_to);
            return out;
        }
    } else {
        /* Default directory: ~/Downloads or cwd */
        const char *home = getenv("HOME");
        if (home)
            snprintf(dir, sizeof(dir), "%s/Downloads", home);
        else
            strcpy(dir, ".");
        struct stat st;
        if (stat(dir, &st) != 0 || !S_ISDIR(st.st_mode))
            strcpy(dir, ".");
    }
    return flux3_free_path(dir, filename);
}

/* PoP: _free_path @ tools/flux3_video_tool.py:_free_path */
char *flux3_free_path(const char *directory, const char *name)
{
    /* candidate = directory / name; if exists, name-2.ext, name-3.ext, ... */
    char candidate[4096];
    snprintf(candidate, sizeof(candidate), "%s/%s", directory ? directory : ".",
             name ? name : "flux3-video.mp4");
    struct stat st;
    if (stat(candidate, &st) != 0) {
        return strdup(candidate);
    }
    /* Extract stem and suffix */
    const char *dot = strrchr(name, '.');
    const char *base = strrchr(name, '/');
    if (base) base++;
    else base = name;
    char stem[256], suffix[32];
    if (dot && dot > base) {
        size_t sl = (size_t)(dot - base);
        if (sl >= sizeof(stem)) sl = sizeof(stem) - 1;
        memcpy(stem, base, sl); stem[sl] = '\0';
        size_t suf_l = strlen(dot);
        if (suf_l >= sizeof(suffix)) suf_l = sizeof(suffix) - 1;
        memcpy(suffix, dot, suf_l); suffix[suf_l] = '\0';
    } else {
        strncpy(stem, base, sizeof(stem) - 1);
        stem[sizeof(stem) - 1] = '\0';
        strcpy(suffix, "");
    }
    for (int s = 2; s < FLUX3_MAX_FILENAME_ATTEMPTS + 2; s++) {
        snprintf(candidate, sizeof(candidate), "%s/%s-%d%s", directory, stem, s, suffix);
        if (stat(candidate, &st) != 0) return strdup(candidate);
    }
    return NULL;
}

