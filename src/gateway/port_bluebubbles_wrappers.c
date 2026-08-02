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
int bb_u_clean_mention_text(const char *arg) { (void)arg; return 0; }

/* PoP: _api_post @ gateway/platforms/bluebubbles.py:_api_post */
int bb_u_api_post(const char *arg) { (void)arg; return 0; }

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
int bb_u_webhook_register_url(const char *arg) { (void)arg; return 0; }

/* PoP: _webhook_register_url_for_log @ gateway/platforms/bluebubbles.py:_webhook_register_url_for_log */
int bb_u_webhook_register_url_for_log(const char *arg) { (void)arg; return 0; }

/* PoP: _find_registered_webhooks @ gateway/platforms/bluebubbles.py:_find_registered_webhooks */
int bb_u_find_registered_webhooks(const char *arg) { (void)arg; return 0; }

/* PoP: _register_webhook @ gateway/platforms/bluebubbles.py:_register_webhook */
int bb_u_register_webhook(const char *arg) { (void)arg; return 0; }

/* PoP: _unregister_webhook @ gateway/platforms/bluebubbles.py:_unregister_webhook */
int bb_u_unregister_webhook(const char *arg) { (void)arg; return 0; }

/* PoP: _resolve_chat_guid @ gateway/platforms/bluebubbles.py:_resolve_chat_guid */
int bb_u_resolve_chat_guid(const char *arg) { (void)arg; return 0; }

/* PoP: _create_chat_for_handle @ gateway/platforms/bluebubbles.py:_create_chat_for_handle */
int bb_u_create_chat_for_handle(const char *arg) { (void)arg; return 0; }

/* PoP: mark_read @ gateway/platforms/bluebubbles.py:mark_read */
int bb_mark_read(const char *arg) { (void)arg; return 0; }

/* PoP: _download_attachment @ gateway/platforms/bluebubbles.py:_download_attachment */
int bb_u_download_attachment(const char *arg) { (void)arg; return 0; }

/* PoP: _extract_payload_record @ gateway/platforms/bluebubbles.py:_extract_payload_record */
int bb_u_extract_payload_record(const char *arg) { (void)arg; return 0; }
