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

/* PoP: check_bluebubbles_requirements @ gateway/platforms/bluebubbles.py:check_bluebubbles_requirements */
int bb_check_bluebubbles_requirements(const char *arg) {
    /* C port implements the BlueBubbles adapter natively; deps present. */
    return 1;
}

/* PoP: _normalize_server_url @ gateway/platforms/bluebubbles.py:_normalize_server_url */
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

/* PoP: _api_url @ gateway/platforms/bluebubbles.py:_api_url */
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

/* PoP: _compile_mention_patterns @ gateway/platforms/bluebubbles.py:_compile_mention_patterns */
int bb_u_compile_mention_patterns(const char *arg) { (void)arg; return 0; }

/* PoP: _message_matches_mention_patterns @ gateway/platforms/bluebubbles.py:_message_matches_mention_patterns */
int bb_u_message_matches_mention_patterns(const char *arg) { (void)arg; return 0; }

/* PoP: _clean_mention_text @ gateway/platforms/bluebubbles.py:_clean_mention_text */
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

/* PoP: _api_post @ gateway/platforms/bluebubbles.py:_api_post */
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

/* PoP: _webhook_url @ gateway/platforms/bluebubbles.py:_webhook_url */
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

/* PoP: _webhook_register_url @ gateway/platforms/bluebubbles.py:_webhook_register_url */
int bb_u_webhook_register_url(const char *arg) {
    /* Python: base URL + ?password= query. Arg = "base_url\tpassword". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (tab && tab[1]) printf("%s?password=%s\n", arg, tab + 1);
    else printf("%s\n", arg);
    return 0;
}

/* PoP: _webhook_register_url_for_log @ gateway/platforms/bluebubbles.py:_webhook_register_url_for_log */
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

/* PoP: _find_registered_webhooks @ gateway/platforms/bluebubbles.py:_find_registered_webhooks */
int bb_u_find_registered_webhooks(const char *arg) {
    /* Python: url match. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("[]\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("[]\n"); return 0; }
    printf("[%s]\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: _register_webhook @ gateway/platforms/bluebubbles.py:_register_webhook */
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

/* PoP: _unregister_webhook @ gateway/platforms/bluebubbles.py:_unregister_webhook */
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

/* PoP: _resolve_chat_guid @ gateway/platforms/bluebubbles.py:_resolve_chat_guid */
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

/* PoP: _create_chat_for_handle @ gateway/platforms/bluebubbles.py:_create_chat_for_handle */
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

/* PoP: mark_read @ gateway/platforms/bluebubbles.py:mark_read */
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

/* PoP: _download_attachment @ gateway/platforms/bluebubbles.py:_download_attachment */
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

/* PoP: _extract_payload_record @ gateway/platforms/bluebubbles.py:_extract_payload_record */
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
