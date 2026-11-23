/*
 * bot_mode_dm_chain.c — C home for upstream Hermes tools/bot_mode_dm_chain.py.
 * PoP: w10_botmodedm_ebba71_definitely_not_sent @ tools/bot_mode_dm_chain.py:_definitely_not_sent
 * PoP: w10_botmodedm_ebba71_peer_key @ tools/bot_mode_dm_chain.py:_peer_key
 */
#include "hermes_json.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w10_botmodedm_ebba71.h"

static int py_truthy(json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count ? 1 : 0;
        default: return 0;
    }
}

int w10_botmodedm_ebba71_definitely_not_sent(json_t *ack) {
    if (!ack || ack->type != JSON_OBJECT) return 0;
    json_t *err = json_obj_get(ack, "error");
    json_t *st = json_obj_get(ack, "status");
    if (!py_truthy(err)) return 0;
    /* ack.get("status") in (None, "failed") */
    if (!st || st->type == JSON_NULL) return 1;
    if (st->type == JSON_STRING && st->str_val && !strcmp(st->str_val, "failed")) return 1;
    return 0;
}

/* urlparse(url).hostname, lowercased; "" when the URL has no authority. */
static char *url_host_lower(const char *url) {
    char host[512] = {0};
    if (!url) return strdup("");
    const char *p = strstr(url, "://");
    if (!p) return strdup("");
    p += 3;
    const char *end = p + strcspn(p, "/?#");
    char auth[512]; size_t n = (size_t)(end - p);
    if (n >= sizeof(auth)) n = sizeof(auth) - 1;
    memcpy(auth, p, n); auth[n] = '\0';
    char *at = strrchr(auth, '@');
    const char *h = at ? at + 1 : auth;
    char *colon = strchr(h, ':');
    size_t hl = colon ? (size_t)(colon - h) : strlen(h);
    if (hl >= sizeof(host)) hl = sizeof(host) - 1;
    memcpy(host, h, hl); host[hl] = '\0';
    for (char *c = host; *c; c++) *c = (char)tolower((unsigned char)*c);
    return strdup(host);
}

char *w10_botmodedm_ebba71_peer_key(const char *peer_name, const char *peer_profile,
                                    json_t *turn_author, const char *peer_url) {
    const char *id = "";
    int is_bot = 0;
    if (turn_author && turn_author->type == JSON_OBJECT) {
        json_t *jid = json_obj_get(turn_author, "id");
        if (jid && jid->type == JSON_STRING && jid->str_val) id = jid->str_val;
        json_t *jb = json_obj_get(turn_author, "is_bot");
        is_bot = py_truthy(jb);
    }
    /* str(id or "").removeprefix("bot:").rpartition("/") */
    const char *base = id;
    if (!strncmp(base, "bot:", 4)) base += 4;
    const char *slash = strrchr(base, '/');
    char origin[512], profile[512];
    if (slash) {
        size_t ol = (size_t)(slash - base);
        if (ol >= sizeof(origin)) ol = sizeof(origin) - 1;
        memcpy(origin, base, ol); origin[ol] = '\0';
        snprintf(profile, sizeof(profile), "%s", slash + 1);
    } else {
        origin[0] = '\0';
        snprintf(profile, sizeof(profile), "%s", base);
    }
    if (is_bot && origin[0] && peer_profile &&
        !_stricmp(profile, peer_profile)) {
        char *host = url_host_lower(peer_url);
        char olow[512]; snprintf(olow, sizeof(olow), "%s", origin);
        for (char *c = olow; *c; c++) *c = (char)tolower((unsigned char)*c);
        char dot[512]; snprintf(dot, sizeof(dot), "%s", host);
        char *d = strchr(dot, '.'); if (d) *d = '\0';
        if (host[0] && (!strcmp(olow, host) || !strcmp(olow, dot))) {
            free(host);
            return strdup(id);
        }
        free(host);
    }
    char out[1024];
    snprintf(out, sizeof(out), "peer:%s/%s", peer_name ? peer_name : "",
             peer_profile ? peer_profile : "");
    for (char *c = out; *c; c++) *c = (char)tolower((unsigned char)*c);
    return strdup(out);
}
