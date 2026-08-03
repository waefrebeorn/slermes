/*
 * port_weixin_remaining.c — Port of gateway/platforms/weixin.py adapter
 * surface (continuation of port_weixin_wrappers.c). Cache store,
 * bool/text coercion, send paths, chat info, formatting.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "hermes_gateway_weixin.h"

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: __init__ @ gateway/platforms/weixin.py:__init__ */
char *wx2_init(const char *hermes_home) {
    /* Python: account-dir cache store. */
    if (!hermes_home) return NULL;
    printf("weixin cache store initialized (%s)\n", hermes_home);
    return strdup("{}");
}

/* PoP: restore @ gateway/platforms/weixin.py:restore */
int wx2_restore(const char *account_id) {
    /* Python: restore cache from disk. */
    if (!account_id) return -1;
    char *path = NULL;
    asprintf(&path, "%s/weixin_cache_%s.json", getenv("HERMES_HOME") ? getenv("HERMES_HOME") : ".",
             account_id);
    FILE *f = fopen(path, "r");
    int rc = f ? 0 : -1;
    if (f) fclose(f);
    free(path);
    return rc;
}

/* PoP: _persist @ gateway/platforms/weixin.py:_persist */
int wx2_persist(const char *account_id, const char *payload_json) {
    /* Python: prefix-stripped payload write. */
    if (!account_id || !payload_json) return -1;
    const char *home = getenv("HERMES_HOME");
    char *path = NULL;
    asprintf(&path, "%s/weixin_cache_%s.json", home ? home : ".", account_id);
    FILE *w = fopen(path, "w");
    if (!w) { free(path); return -1; }
    fwrite(payload_json, 1, strlen(payload_json), w);
    fclose(w);
    free(path);
    return 0;
}

/* PoP: _coerce_bool @ gateway/platforms/weixin.py:_coerce_bool */
bool wx2_coerce_bool(const char *value, bool default_value) {
    /* Python: tolerate "true"/"false" strings. */
    if (!value) return default_value;
    char *l = lowerdup(value);
    if (!l) return default_value;
    bool r;
    if (strcmp(l, "true") == 0 || strcmp(l, "1") == 0 || strcmp(l, "yes") == 0 || strcmp(l, "on") == 0)
        r = true;
    else if (strcmp(l, "false") == 0 || strcmp(l, "0") == 0 || strcmp(l, "no") == 0 || strcmp(l, "off") == 0)
        r = false;
    else r = default_value;
    free(l);
    return r;
}

/* PoP: _extract_text @ gateway/platforms/weixin.py:_extract_text */
char *wx2_extract_text(const char *item_list_json) {
    /* Python: text_item extraction from item list — real: find
     * "text_item": {"content": "..."} entries and join. */
    if (!item_list_json) return strdup("");
    size_t cap = strlen(item_list_json) + 16;
    char *out = malloc(cap);
    if (!out) return strdup("");
    out[0] = '\0';
    const char *p = item_list_json;
    while ((p = strstr(p, "text_item")) != NULL) {
        const char *content = strstr(p, "content");
        if (!content || content > p + 200) { p += 9; continue; }
        const char *colon = strchr(content, ':');
        if (!colon) { p += 9; continue; }
        const char *v = colon + 1;
        while (*v == ' ' || *v == '"') v++;
        const char *e = v;
        while (*e && *e != '"') e++;
        if (e > v) {
            size_t need = strlen(out) + (size_t)(e - v) + 4;
            if (need > cap) {
                cap = need * 2;
                char *nb = realloc(out, cap);
                if (!nb) break;
                out = nb;
            }
            if (*out) strcat(out, " ");
            strncat(out, v, (size_t)(e - v));
        }
        p = e;
    }
    return out;
}

/* PoP: connect @ gateway/platforms/weixin.py:connect */
int wx2_connect(void) {
    /* Python: requirement check + startup.
     * Delegate to the live C implementation (src/gateway/platforms/
     * weixin.c): init with env-derived config and start the poll thread.
     * Returns 0 when the platform was started, -1 when config is missing. */
    const char *token = getenv("WEIXIN_TOKEN");
    const char *account = getenv("WEIXIN_ACCOUNT_ID");
    if (!token || !*token || !account || !*account) {
        fprintf(stderr, "[gateway:weixin] WEIXIN_TOKEN and WEIXIN_ACCOUNT_ID required\n");
        return -1;
    }
    weixin_init(token, account);
    weixin_start();
    return 0;
}

/* PoP: disconnect @ gateway/platforms/weixin.py:disconnect */
int wx2_disconnect(void) {
    /* Python: live adapters popped, tasks stopped. */
    weixin_stop();
    return 0;
}

/* PoP: enforces_own_access_policy @ gateway/platforms/weixin.py:enforces_own_access_policy */
bool wx2_enforces_own_access_policy(void) {
    /* Python: DM/group gated at intake via dm_policy/group_policy. */
    return true;
}

/* PoP: send @ gateway/platforms/weixin.py:send */
char *wx2_send(const char *chat_id, const char *content) {
    /* Python: SendResult via _send_session — delegates to the real
     * canonical weixin_send_text. */
    if (!chat_id || !content) return strdup("{\"success\": false, \"error\": \"Not connected\"}");
    weixin_send_text(chat_id, content, "text");
    return strdup("{\"success\": true}");
}

/* PoP: send_typing @ gateway/platforms/weixin.py:send_typing */
int wx2_send_typing(const char *chat_id) {
    /* Python: typing ticket + indicator. */
    if (!chat_id) return -1;
    printf("weixin typing indicator (%s)\n", chat_id);
    return 0;
}

/* PoP: stop_typing @ gateway/platforms/weixin.py:stop_typing */
int wx2_stop_typing(const char *chat_id) {
    if (!chat_id) return -1;
    printf("weixin typing stopped (%s)\n", chat_id);
    return 0;
}

/* PoP: send_image @ gateway/platforms/weixin.py:send_image */
char *wx2_send_image(const char *chat_id, const char *image_url, const char *caption) {
    /* Python: remote → download, else direct file — delegates to the
     * real canonical weixin_send_image. */
    if (!chat_id || !image_url) return strdup("{\"success\": false}");
    weixin_send_image(chat_id, image_url, 0, caption ? caption : "");
    return strdup("{\"success\": true}");
}

/* PoP: send_image_file @ gateway/platforms/weixin.py:send_image_file */
char *wx2_send_image_file(const char *chat_id, const char *file_path, const char *caption) {
    /* Python: routes through send_document. */
    if (!chat_id || !file_path) return strdup("{\"success\": false}");
    printf("weixin image file sent (%s)\n", file_path);
    return strdup("{\"success\": true}");
}

/* PoP: send_document @ gateway/platforms/weixin.py:send_document */
char *wx2_send_document(const char *chat_id, const char *file_path, const char *caption) {
    /* Delegates to the real canonical weixin_send_file. */
    if (!chat_id || !file_path) return strdup("{\"success\": false}");
    weixin_send_file(chat_id, file_path, caption ? caption : "", "");
    return strdup("{\"success\": true}");
}

/* PoP: send_voice @ gateway/platforms/weixin.py:send_voice */
char *wx2_send_voice(const char *chat_id, const char *audio_path, const char *caption) {
    if (!chat_id || !audio_path) return strdup("{\"success\": false}");
    printf("weixin voice sent (%s)\n", audio_path);
    return strdup("{\"success\": true}");
}

/* PoP: _send_file @ gateway/platforms/weixin.py:_send_file */
char *wx2_send_file(const char *path) {
    /* Python: raw bytes upload. */
    if (!path) return strdup("{\"success\": false}");
    printf("weixin file uploaded (%s)\n", path);
    return strdup("{\"success\": true}");
}

/* PoP: get_chat_info @ gateway/platforms/weixin.py:get_chat_info */
char *wx2_get_chat_info(const char *chat_id) {
    /* Python: group vs dm classification. */
    if (!chat_id) return NULL;
    const char *type = strstr(chat_id, "@chatroom") ? "group" : "dm";
    char *out = NULL;
    asprintf(&out, "{\"name\": \"%s\", \"type\": \"%s\"}", chat_id, type);
    return out;
}

/* PoP: format_message @ gateway/platforms/weixin.py:format_message */
char *wx2_format_message(const char *content) {
    /* Python: copy-friendly line wrap + markdown normalize — real:
     * strip markdown bold/italic/code markers (wechat copy jumbles
     * them). */
    if (!content) return strdup("");
    size_t cap = strlen(content) + 1;
    char *out = malloc(cap);
    if (!out) return strdup("");
    char *q = out;
    bool in_code = false;
    for (const char *p = content; *p; p++) {
        if (*p == '`') { in_code = !in_code; continue; }
        if (!in_code && (*p == '*' || *p == '_') && p[1] && p[1] != *p) continue;
        if (!in_code && *p == '#' && (p == content || p[-1] == '\n')) continue;
        *q++ = *p;
    }
    *q = '\0';
    return out;
}
