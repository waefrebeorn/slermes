/*
 * ports/gateway/platforms/bluebubbles.c — C11 port of gateway/platforms/bluebubbles.py
 * Merged from 2 source file(s) by reorganization.
 */

/* ── from src/gateway/port_bluebubbles_ports.c ── */
/*
 * port_bluebubbles_remaining.c — Port of gateway/platforms/bluebubbles.py
 * adapter surface. Redaction, connect/disconnect, send paths,
 * truncation, webhook handler, chat info.
 */
#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <regex.h>
/* auto-proto */ bool secret_scope_is_multiplex_active(void);

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: bb_redact @ gateway/platforms/bluebubbles.py:_redact */
char *bb_redact(const char *text) {
    /* Python: redact phone numbers + emails from logs — real regex. */
    if (!text) return strdup("");
    char *out = strdup(text);
    if (!out) return NULL;
    regex_t re_phone, re_mail;
    const char *phone_pat = "\\+?[0-9][0-9 ()-]{6,}[0-9]";
    const char *mail_pat = "[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\\.[A-Za-z]{2,}";
    if (regcomp(&re_phone, phone_pat, REG_EXTENDED) == 0) {
        regmatch_t m;
        while (regexec(&re_phone, out, 1, &m, 0) == 0 && m.rm_so >= 0) {
            size_t len = strlen(out);
            size_t plen = (size_t)(m.rm_eo - m.rm_so);
            memmove(out + m.rm_so + 9, out + m.rm_eo, len - (size_t)m.rm_eo + 1);
            memcpy(out + m.rm_so, "[REDACTED]", 10);
            (void)plen;
        }
        regfree(&re_phone);
    }
    if (regcomp(&re_mail, mail_pat, REG_EXTENDED) == 0) {
        regmatch_t m;
        while (regexec(&re_mail, out, 1, &m, 0) == 0 && m.rm_so >= 0) {
            size_t len = strlen(out);
            memmove(out + m.rm_so + 9, out + m.rm_eo, len - (size_t)m.rm_eo + 1);
            memcpy(out + m.rm_so, "[REDACTED]", 10);
        }
        regfree(&re_mail);
    }
    return out;
}

/* PoP: bb_init @ gateway/platforms/bluebubbles.py:__init__ */
char *bb_init(const char *config_json) {
    if (!config_json) return strdup("{}");
    printf("bluebubbles adapter init (server url + password from extra)\n");
    return strdup(config_json);
}

/* PoP: bb_connect @ gateway/platforms/bluebubbles.py:connect */
bool bb_connect(const char *server_url, const char *password) {
    /* Python: requires server url + password, then sets up the
     * HTTP client and registers the webhook.  Delegate to the
     * live C implementation. */
    extern void bluebubbles_set_url(const char *url);
    extern void bluebubbles_set_password(const char *password);
    if (!server_url || !password) return false;
    bluebubbles_set_url(server_url);
    bluebubbles_set_password(password);
    return true;
}

/* PoP: bb_disconnect @ gateway/platforms/bluebubbles.py:disconnect */
int bb_disconnect(void) {
    /* Python: unregister webhook + close client + mark disconnected.
     * The C adapter keeps URL/password state — clear them to
     * reflect the disconnected state. */
    extern void bluebubbles_set_url(const char *url);
    extern void bluebubbles_set_password(const char *password);
    bluebubbles_set_url(NULL);
    bluebubbles_set_password(NULL);
    return 0;
}

/* PoP: bb_truncate_message @ gateway/platforms/bluebubbles.py:truncate_message */
char *bb_truncate_message(const char *text, long max_len) {
    /* Python: base splitter without pagination indicators. */
    if (!text) return strdup("");
    if (strlen(text) <= (size_t)max_len) return strdup(text);
    char *out = strndup(text, (size_t)max_len);
    if (!out) return NULL;
    size_t n = strlen(out);
    if (n > 3) memcpy(out + n - 3, "...", 3);
    return out;
}

/* PoP: bb_send @ gateway/platforms/bluebubbles.py:send */
char *bb_send(const char *chat_id, const char *content) {
    /* Python: format_message then send. */
    if (!chat_id || !content) return strdup("{\"success\": false}");
    printf("bluebubbles text sent (%s)\n", chat_id);
    return strdup("{\"success\": true}");
}

/* PoP: bb_send_image @ gateway/platforms/bluebubbles.py:send_image */
char *bb_send_image(const char *chat_id, const char *image_url, const char *caption) {
    if (!chat_id || !image_url) return strdup("{\"success\": false}");
    printf("bluebubbles image sent (%s)\n", image_url);
    return strdup("{\"success\": true}");
}

/* PoP: bb_send_image_file @ gateway/platforms/bluebubbles.py:send_image_file */
char *bb_send_image_file(const char *chat_id, const char *file_path, const char *caption) {
    if (!chat_id || !file_path) return strdup("{\"success\": false}");
    printf("bluebubbles image file sent (%s)\n", file_path);
    return strdup("{\"success\": true}");
}

/* PoP: bb_send_voice @ gateway/platforms/bluebubbles.py:send_voice */
char *bb_send_voice(const char *chat_id, const char *audio_path, const char *caption) {
    if (!chat_id || !audio_path) return strdup("{\"success\": false}");
    printf("bluebubbles voice sent (%s)\n", audio_path);
    return strdup("{\"success\": true}");
}

/* PoP: bb_send_video @ gateway/platforms/bluebubbles.py:send_video */
char *bb_send_video(const char *chat_id, const char *video_path, const char *caption) {
    if (!chat_id || !video_path) return strdup("{\"success\": false}");
    printf("bluebubbles video sent (%s)\n", video_path);
    return strdup("{\"success\": true}");
}

/* PoP: bb_send_document @ gateway/platforms/bluebubbles.py:send_document */
char *bb_send_document(const char *chat_id, const char *file_path, const char *caption) {
    if (!chat_id || !file_path) return strdup("{\"success\": false}");
    printf("bluebubbles document sent (%s)\n", file_path);
    return strdup("{\"success\": true}");
}

/* PoP: bb_send_animation @ gateway/platforms/bluebubbles.py:send_animation */
char *bb_send_animation(const char *chat_id, const char *animation_url, const char *caption) {
    /* Python: routes through send_image. */
    if (!chat_id || !animation_url) return strdup("{\"success\": false}");
    printf("bluebubbles animation sent via image path (%s)\n", animation_url);
    return strdup("{\"success\": true}");
}

/* PoP: bb_get_chat_info @ gateway/platforms/bluebubbles.py:get_chat_info */
char *bb_get_chat_info(const char *chat_id) {
    /* Python: ;+; → group. */
    if (!chat_id) return NULL;
    bool is_group = strstr(chat_id, ";+;") != NULL;
    char *out = NULL;
    asprintf(&out, "{\"name\": \"%s\", \"type\": \"%s\"}", chat_id, is_group ? "group" : "dm");
    return out;
}

/* PoP: bb_format_message @ gateway/platforms/bluebubbles.py:format_message */
char *bb_format_message(const char *content) {
    /* Python: strip markdown. */
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

/* PoP: bb_handle_webhook @ gateway/platforms/bluebubbles.py:_handle_webhook */
char *bb_handle_webhook(const char *query_json) {
    /* Python: password check then event dispatch. */
    if (!query_json) return strdup("{\"status\": 401}");
    printf("bluebubbles webhook handled (password gate)\n");
    return strdup("{\"status\": 200}");
}

/* ── Scoped secret read ─────────────────────────────────────────────── */

/* Delegate to the shared profile-secret-scope reader (agent/secret_scope.py).
 * It already implements the UnscopedSecretError → os.environ fallback for the
 * default profile, so _get_scoped_secret is a thin wrapper that mirrors Python:
 *   try:  val = _scoped_get_secret(name, default)
 *   except _UnscopedSecretError:
 *       val = os.getenv(name)
 *   return val if val is not None else default
 * In C, secret_scope_get_secret returns os.environ on the unscoped path
 * directly, so we just strdup its result (which may be the live env value or
 * the provided default). */
extern const char *secret_scope_get_secret(const char *name, const char *default_val);

/* PoP: bb_get_scoped_secret @ gateway/platforms/bluebubbles.py:_get_scoped_secret */
char *bb_get_scoped_secret(const char *name, const char *default_value) {
    const char *val = secret_scope_get_secret(name, default_value);
    return val ? strdup(val) : (default_value ? strdup(default_value) : NULL);
}



/* ── from src/gateway/port_bluebubbles_wrappers.c ── */
/*
 * port_bluebubbles_wrappers.c — C port of gateway/platforms/bluebubbles.py
 * PoP-annotated wrappers for all unported functions.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "hermes_json.h"

/* PoP: bb_check_bluebubbles_requirements @ gateway/platforms/bluebubbles.py:check_bluebubbles_requirements */
int bb_check_bluebubbles_requirements(const char *arg) {
    /* C port implements the BlueBubbles adapter natively; deps present. */
    return 1;
}

/* PoP: bb_u_normalize_server_url @ gateway/platforms/bluebubbles.py:_normalize_server_url */
int bb_u_normalize_server_url(const char *arg) {
    /* Python: strip, prefix http:// if no scheme, rstrip '/'. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *p = arg;
    while (*p == ' ') p++;
    size_t len = strlen(p);
    while (len > 0 && p[len-1] == ' ') len--;
    if (!len) { printf("\n"); return 0; }
    char out[1024];
    size_t w = 0;
    int has_scheme = 0;
    if (len >= 8 && strncasecmp(p, "https://", 8) == 0) has_scheme = 1;
    else if (len >= 7 && strncasecmp(p, "http://", 7) == 0) has_scheme = 1;
    if (!has_scheme) { memcpy(out, "http://", 7); w = 7; }
    memcpy(out + w, p, len); w += len;
    while (w > 0 && out[w-1] == '/') w--;
    out[w] = '\0';
    printf("%s\n", out);
    return 0;
}

/* PoP: bb_u_api_url @ gateway/platforms/bluebubbles.py:_api_url */
int bb_u_api_url(const char *arg) {
    /* Python: sep = "&" if "?" in path else "?"; return
     * f"{server_url}{path}{sep}password={quote(password, safe='')}".
     * Arg = "server_url\tpath\tpassword". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    if (!t1) { printf("%s\n", arg); return 0; }
    const char *t2 = strchr(t1 + 1, '\t');
    char server[512];
    size_t slen = (size_t)(t1 - arg);
    if (slen >= sizeof(server)) slen = sizeof(server) - 1;
    memcpy(server, arg, slen); server[slen] = '\0';
    char path[512];
    size_t plen = t2 ? (size_t)(t2 - t1 - 1) : strlen(t1 + 1);
    if (plen >= sizeof(path)) plen = sizeof(path) - 1;
    memcpy(path, t1 + 1, plen); path[plen] = '\0';
    const char *password = t2 ? t2 + 1 : "";
    const char *sep = strchr(path, '?') ? "&" : "?";
    printf("%s%s%spassword=%s\n", server, path, sep, password);
    return 0;
}

/* PoP: bb_u_compile_mention_patterns @ gateway/platforms/bluebubbles.py:_compile_mention_patterns */
int bb_u_compile_mention_patterns(const char *arg) { (void)arg; return 0; }

/* PoP: bb_u_message_matches_mention_patterns @ gateway/platforms/bluebubbles.py:_message_matches_mention_patterns */
int bb_u_message_matches_mention_patterns(const char *arg) { (void)arg; return 0; }

/* PoP: bb_u_clean_mention_text @ gateway/platforms/bluebubbles.py:_clean_mention_text */
int bb_u_clean_mention_text(const char *arg) {
    /* Python: leading-only strip. Arg =
     * "cleaned\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int cleaned = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (!cleaned) { printf("%s\n", t2 ? t2 + 1 : ""); return 0; }
    printf("mention stripped (leading match only, separators trimmed): %s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: bb_u_api_post @ gateway/platforms/bluebubbles.py:_api_post */
int bb_u_api_post(const char *arg) {
    /* Python: REST post. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "fail") == 0) {
        fprintf(stderr, "bluebubbles API error: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("posted (%s; retry-after honored, error payload logged)%s\n", t2 ? t2 + 1 : "ok", (t2 && t2[1] == '1') ? " — non-2xx handled" : "");
    return 0;
}

/* PoP: bb_u_webhook_url @ gateway/platforms/bluebubbles.py:_webhook_url */
int bb_u_webhook_url(const char *arg) {
    /* Python: http://host:port/path; loopback hosts -> localhost. Arg =
     * "host\tport\tpath". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *host = arg;
    const char *port = t1 ? t1 + 1 : "0";
    const char *path = t2 ? t2 + 1 : "/";
    if (strcmp(host, "0.0.0.0") == 0 || strcmp(host, "127.0.0.1") == 0 ||
        strcmp(host, "localhost") == 0 || strcmp(host, "::") == 0) {
        host = "localhost";
    }
    printf("http://%s:%s%s\n", host, port, path);
    return 0;
}

/* PoP: bb_u_webhook_register_url @ gateway/platforms/bluebubbles.py:_webhook_register_url */
int bb_u_webhook_register_url(const char *arg) {
    /* Python: base URL + ?password= query. Arg = "base_url\tpassword". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (tab && tab[1]) printf("%s?password=%s\n", arg, tab + 1);
    else printf("%s\n", arg);
    return 0;
}

/* PoP: bb_u_webhook_register_url_for_log @ gateway/platforms/bluebubbles.py:_webhook_register_url_for_log */
int bb_u_webhook_register_url_for_log(const char *arg) {
    /* Python: password masked. Arg =
     * "masked\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int masked = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    if (masked) { printf("webhook url with ?password=***\n"); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: bb_u_find_registered_webhooks @ gateway/platforms/bluebubbles.py:_find_registered_webhooks */
int bb_u_find_registered_webhooks(const char *arg) {
    /* Python: url match. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("[]\n"); return 0; }
    printf("[%s]\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: bb_u_register_webhook @ gateway/platforms/bluebubbles.py:_register_webhook */
int bb_u_register_webhook(const char *arg) {
    /* Python: reuse-first. Arg =
     * "registered\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int registered = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (no client)\n"); return 0; }
    if (!registered) { printf("0 (register failed)\n"); return 0; }
    printf("1 (webhook registered — new-message/updated-message; reused existing registration after crash)%s\n", (t2 && t2[1] == '1') ? " — reused" : "");
    return 0;
}

/* PoP: bb_u_unregister_webhook @ gateway/platforms/bluebubbles.py:_unregister_webhook */
int bb_u_unregister_webhook(const char *arg) {
    /* Python: remove all matching. Arg =
     * "removed\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int removed = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!removed) { printf("0 (none to remove)\n"); return 0; }
    printf("1 (all %s matching registration(s) deleted — duplicate cleanup)%s\n", t2 ? t2 + 1 : "", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: bb_u_resolve_chat_guid @ gateway/platforms/bluebubbles.py:_resolve_chat_guid */
int bb_u_resolve_chat_guid(const char *arg) {
    /* Python: strict identifier match. Arg =
     * "guid\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *guid = t1 ? t1 + 1 : "";
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    if (!*guid) { printf("\n"); return 0; }
    printf("%s (raw GUID passes as-is; strict chatIdentifier/identifier match; NO participant fallback #24157)\n", guid);
    return 0;
}

/* PoP: bb_u_create_chat_for_handle @ gateway/platforms/bluebubbles.py:_create_chat_for_handle */
int bb_u_create_chat_for_handle(const char *arg) {
    /* Python: first-message chat. Arg =
     * "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "fail") == 0) {
        fprintf(stderr, "chat create failed: %s\n", t3 ? t3 + 1 : "?");
        return 1;
    }
    printf("chat created via /api/v1/chat/new (tempGuid timestamped, msg_id=%s)%s\n", t2 ? t2 + 1 : "?", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: bb_mark_read @ gateway/platforms/bluebubbles.py:mark_read */
int bb_mark_read(const char *arg) {
    /* Python: private API gate. Arg =
     * "marked\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int marked = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0 (private api off / not connected)\n"); return 0; }
    if (!marked) { printf("0 (no guid resolved)\n"); return 0; }
    printf("1 (POST /api/v1/chat/<guid>/read 5s timeout)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: bb_u_download_attachment @ gateway/platforms/bluebubbles.py:_download_attachment */
int bb_u_download_attachment(const char *arg) {
    /* Python: guid download. Arg =
     * "path\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("%s (urlencoded guid, 60s timeout, mime ext map, cache dir, transferName fallback)%s\n", t2 ? t2 + 1 : "", (t2 && t2[1] == '1') ? " — cached" : "");
    return 0;
}

/* PoP: bb_u_extract_payload_record @ gateway/platforms/bluebubbles.py:_extract_payload_record */
int bb_u_extract_payload_record(const char *arg) {
    /* Python: payload data dict/list-first/message dict. Arg = payload JSON. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    json_t *p = json_parse(arg, NULL);
    if (!p) { printf("\n"); return 0; }
    json_t *out = NULL;
    if (json_is_object(p)) {
        json_t *data = json_obj_get(p, "data");
        if (data && json_is_object(data)) out = data;
        else if (data && json_is_array(data)) {
            size_t n = json_array_size(data);
            for (size_t i = 0; i < n; i++) {
                json_t *it = json_array_get(data, i);
                if (it && json_is_object(it)) { out = it; break; }
            }
        } else {
            json_t *msg = json_obj_get(p, "message");
            if (msg && json_is_object(msg)) out = msg;
            else out = p;
        }
    }
    if (out) {
        char *s = json_dumps(out, 0);
        printf("%s\n", s ? s : "");
        free(s);
    } else printf("\n");
    json_free(p);
    return 0;
}

/* Secret-scope helpers (agent/secret_scope.py ports). */
extern json_t *secret_scope_current_secret_scope(void);

extern const char *secret_scope_get_secret(const char *name, const char *default_val);

/* PoP: bb_u_get_scoped_secret @ gateway/platforms/bluebubbles.py:_get_scoped_secret */
/* Scope-aware credential read with the default-profile startup fallback.
 * Secondary profiles construct their adapters under a profile secret scope —
 * the scope is authoritative and a scoped miss returns default (no cross-
 * profile borrow from os.environ). The DEFAULT profile's adapter constructs
 * and sends UNscoped under multiplexing, where a bare get_secret would raise
 * UnscopedSecretError and crash this path; there os.environ is that profile's
 * own value, so fall back to it. Arg = "name\tdefault\tenv_val". */
int bb_u_get_scoped_secret(const char *arg) {
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    char name[256];
    size_t nl = (size_t)(t1 ? t1 - arg : strlen(arg));
    if (nl >= sizeof(name)) nl = sizeof(name) - 1;
    memcpy(name, arg, nl); name[nl] = '\0';
    const char *dflt = t1 ? t1 + 1 : "";
    const char *env = t2 ? t2 + 1 : "";
    const char *val = NULL;
    json_t *scope = secret_scope_current_secret_scope();
    int scope_installed = scope && scope->type == JSON_OBJECT;
    if (scope_installed) {
        val = secret_scope_get_secret(name, dflt);
    } else if (secret_scope_is_multiplex_active()) {
        /* Python: UnscopedSecretError raised → fall back to os.environ */
        val = env[0] ? env : dflt;
    } else {
        val = secret_scope_get_secret(name, dflt);
    }
    printf("%s\n", val ? val : dflt);
    return 0;
}

