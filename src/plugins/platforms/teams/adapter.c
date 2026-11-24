/*
 * adapter.c - C11 home for upstream plugins/platforms/teams/adapter.py.
 *
 * Two differential gates share this one port file:
 *   w29_adapter_1c52a4_p1  transport/security/config predicates + constants
 *   w29_adapter_1c52a4_p2  message/card shaping
 *
 * PoP: w29_adapter_1c52a4_p1_allowed_host      @ plugins/platforms/teams/adapter.py:_is_allowed_https_host
 * PoP: w29_adapter_1c52a4_p1_attachment_url    @ plugins/platforms/teams/adapter.py:_is_botframework_attachment_url
 * PoP: w29_adapter_1c52a4_p1_service_url       @ plugins/platforms/teams/adapter.py:_validate_teams_service_url
 * PoP: w29_adapter_1c52a4_p1_conv_id_valid     @ plugins/platforms/teams/adapter.py:_TEAMS_CONV_ID_RE match (_standalone_send / TeamsAdapter._on_card_action)
 * PoP: w29_adapter_1c52a4_p1_token_request     @ plugins/platforms/teams/adapter.py:_bf_token_request
 * PoP: w29_adapter_1c52a4_p1_require_mention   @ plugins/platforms/teams/adapter.py:TeamsAdapter._parse_require_mention
 * PoP: w29_adapter_1c52a4_p1_card_action_denied @ plugins/platforms/teams/adapter.py:TeamsAdapter._card_action_denied
 * PoP: w29_adapter_1c52a4_p1_constants         @ plugins/platforms/teams/adapter.py:<module constants + TeamsAdapter class constants>
 * PoP: w29_adapter_1c52a4_p2_mentions_bot      @ plugins/platforms/teams/adapter.py:TeamsAdapter._activity_mentions_bot
 * PoP: w29_adapter_1c52a4_p2_approval_body     @ plugins/platforms/teams/adapter.py:_approval_body
 * PoP: w29_adapter_1c52a4_p2_media_type        @ plugins/platforms/teams/adapter.py:TeamsAdapter._on_message media-kind precedence (msg_type = next(...))
 * PoP: w29_adapter_1c52a4_p2_attachment_skip   @ plugins/platforms/teams/adapter.py:TeamsAdapter._cache_attachment body/card skip predicate
 * PoP: w29_adapter_1c52a4_p2_document_filename @ plugins/platforms/teams/adapter.py:TeamsAdapter._cache_attachment download-info filename
 * PoP: w29_adapter_1c52a4_p2_threaded_reply    @ plugins/platforms/teams/adapter.py:TeamsAdapter.send reply_to predicate
 *
 * Seams left in the caller: the aiohttp webhook server, App.send/App.reply, the
 * Bot Framework STS token POST, attachment download + media cache, the approval
 * store, the i18n catalog (t), profile-scoped env reads, and the SDK card types
 * (AdaptiveCard/TextBlock/ExecuteAction).  What is ported is what those values
 * decide.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w29_adapter_1c52a4_p1.h"
#include "noc_w29_adapter_1c52a4_p2.h"

/* _ALLOWED_TEAMS_SERVICE_HOSTS (sorted, as the sorted() diagnostic renders it) */
#define TA_ALLOWED_HOSTS_N 2
static const char *const TA_ALLOWED_HOSTS[TA_ALLOWED_HOSTS_N] = {
    "smba.infra.gov.teams.microsoft.us",
    "smba.trafficmanager.net",
};

/* ------------------------------------------------------------------ utils */

static char *ta_dup(const char *s) {
    size_t n;
    char *d;
    if (!s) s = "";
    n = strlen(s);
    d = (char *)malloc(n + 1);
    if (d) memcpy(d, s, n + 1);
    return d;
}

static char *ta_dup_range(const char *s, const char *end) {
    size_t n = (size_t)(end - s);
    char *d = (char *)malloc(n + 1);
    if (d) { memcpy(d, s, n); d[n] = '\0'; }
    return d;
}

static int ta_is_alpha(int c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static int ta_is_scheme_char(int c) {
    return ta_is_alpha(c) || (c >= '0' && c <= '9') || c == '+' || c == '-' || c == '.';
}

/* Python str.isspace() over the ASCII set. */
static int ta_is_space(int c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

static const char *ta_skip_space(const char *s) {
    if (!s) return "";
    while (*s && ta_is_space((unsigned char)*s)) s++;
    return s;
}

static char *ta_trimmed_copy(const char *s) {
    const char *b = ta_skip_space(s);
    size_t n = strlen(b);
    char *d;
    while (n > 0 && ta_is_space((unsigned char)b[n - 1])) n--;
    d = (char *)malloc(n + 1);
    if (d) { memcpy(d, b, n); d[n] = '\0'; }
    return d;
}

/* Python str(value) for the scalar shapes a config value can hold. */
static char *ta_scalar_str(const json_t *v) {
    char buf[64];
    if (!v) return ta_dup("");
    switch (v->type) {
    case JSON_STRING: return ta_dup(v->str_val ? v->str_val : "");
    case JSON_NUMBER:
        if (v->num_val == (double)(long long)v->num_val &&
            v->num_val > -1e15 && v->num_val < 1e15) {
            snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
        } else {
            snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        }
        return ta_dup(buf);
    case JSON_BOOL: return ta_dup(v->bool_val ? "True" : "False");
    case JSON_NULL: return ta_dup("None");
    default: return ta_dup("");
    }
}

/* Python truthiness of a JSON value. */
static int ta_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

/* ------------------------------------------------------- urllib.urlsplit */

/* Python parsed.port for the port slice: -1 == None, -2 == ValueError. */
static long ta_port_of(const char *s) {
    long v = 0;
    const char *q;
    if (!s || !*s) return -1;
    for (q = s; *q; q++) {
        if (*q < '0' || *q > '9') return -2;
        v = v * 10 + (*q - '0');
        if (v > 1000000L) return -2;
    }
    return v > 65535L ? -2 : v;
}

/* urlparse() reduced to what _is_allowed_https_host reads: 1 when the scheme is
 * https and a netloc exists, *host_out = lowercase hostname (malloc'd),
 * *port_out = -1 (None), -2 (ValueError) or the port number. */
static int ta_parse_https(const char *url, char **host_out, long *port_out) {
    const char *p, *net_end, *hostinfo, *at, *hend;
    char *netloc, *host;
    *host_out = NULL;
    *port_out = -1;
    if (!url || !*url || !ta_is_alpha((unsigned char)url[0])) return 0;
    p = url + 1;
    while (*p && ta_is_scheme_char((unsigned char)*p)) p++;
    if (*p != ':' || (size_t)(p - url) != 5) return 0;
    if (url[0] != 'h' && url[0] != 'H') return 0;
    if (url[1] != 't' && url[1] != 'T') return 0;
    if (url[2] != 't' && url[2] != 'T') return 0;
    if (url[3] != 'p' && url[3] != 'P') return 0;
    if (url[4] != 's' && url[4] != 'S') return 0;
    p++;
    if (p[0] != '/' || p[1] != '/') return 0;
    p += 2;
    net_end = p;
    while (*net_end && *net_end != '/' && *net_end != '?' && *net_end != '#') net_end++;
    netloc = ta_dup_range(p, net_end);
    if (!netloc) return 0;
    at = strrchr(netloc, '@');
    hostinfo = at ? at + 1 : netloc;
    if (strchr(hostinfo, '[') || strchr(hostinfo, ']')) {
        if (!strchr(hostinfo, '[') || !strchr(hostinfo, ']')) { /* invalid IPv6 URL */
            free(netloc);
            return 0;
        }
        hend = strchr(hostinfo, ']');
        host = ta_dup_range(hostinfo + 1, hend);
        if (hend[1] == ':') *port_out = ta_port_of(hend + 2);
        else if (hend[1] != '\0') { free(host); free(netloc); return 0; }
    } else {
        const char *colon = strchr(hostinfo, ':');
        if (colon) {
            host = ta_dup_range(hostinfo, colon);
            *port_out = ta_port_of(colon + 1);
        } else {
            host = ta_dup(hostinfo);
        }
    }
    free(netloc);
    if (!host) return 0;
    for (p = host; *p; p++)
        if (*p >= 'A' && *p <= 'Z') ((char *)p)[0] = (char)(*p - 'A' + 'a');
    if (!*host) { free(host); return 0; }
    *host_out = host;
    return 1;
}

int w29_adapter_1c52a4_p1_allowed_host(const char *url, int check_port) {
    char *host = NULL;
    long port = -1;
    int ok = 0;
    size_t i;
    if (!ta_parse_https(url, &host, &port)) return 0;
    if (check_port && port == -2) { free(host); return 0; }
    if (check_port && port != -1 && port != 443) { free(host); return 0; }
    for (i = 0; i < TA_ALLOWED_HOSTS_N; i++)
        if (!strcmp(host, TA_ALLOWED_HOSTS[i])) ok = 1;
    free(host);
    return ok;
}

int w29_adapter_1c52a4_p1_attachment_url(const char *url) {
    return w29_adapter_1c52a4_p1_allowed_host(url, 1);
}

json_t *w29_adapter_1c52a4_p1_service_url(const char *raw) {
    size_t n;
    char *out;
    if (!raw || !*raw) return json_null();
    if (!w29_adapter_1c52a4_p1_allowed_host(raw, 0)) return json_null();
    n = strlen(raw);
    if (raw[n - 1] == '/') return json_string(raw);
    out = (char *)malloc(n + 2);
    if (!out) return json_null();
    memcpy(out, raw, n);
    out[n] = '/';
    out[n + 1] = '\0';
    { json_t *v = json_string(out); free(out); return v; }
}

/* _TEAMS_CONV_ID_RE = ^[A-Za-z0-9:@\-_.]+$ ; re.match, so "$" also matches just
 * before one trailing newline. */
static int ta_conv_char(int c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
           c == ':' || c == '@' || c == '-' || c == '_' || c == '.';
}

int w29_adapter_1c52a4_p1_conv_id_valid(const char *chat_id) {
    size_t i, n, m;
    if (!chat_id) return 0;
    n = strlen(chat_id);
    if (n == 0) return 0;
    m = n;
    if (chat_id[n - 1] == '\n') m = n - 1;
    if (m == 0) return 0;
    for (i = 0; i < m; i++)
        if (!ta_conv_char((unsigned char)chat_id[i])) return 0;
    return 1;
}

json_t *w29_adapter_1c52a4_p1_token_request(const char *tenant_id,
                                            const char *client_id,
                                            const char *client_secret) {
    json_t *out = json_object();
    json_t *form = json_object();
    size_t n = strlen(tenant_id ? tenant_id : "None") + 80;
    char *url = (char *)malloc(n);
    if (!url) { json_free(out); json_free(form); return json_null(); }
    snprintf(url, n, "https://login.microsoftonline.com/%s/oauth2/v2.0/token",
             tenant_id ? tenant_id : "None");
    json_set(out, "url", json_string(url));
    free(url);
    json_set(form, "grant_type", json_string("client_credentials"));
    json_set(form, "client_id", json_string(client_id ? client_id : "None"));
    json_set(form, "client_secret", json_string(client_secret ? client_secret : "None"));
    json_set(form, "scope", json_string("https://api.botframework.com/.default"));
    json_set(out, "form", form);
    return out;
}

/* _extra_or_secret: env rung (non-blank) -> extra[key] (None/blank miss) -> default. */
static const json_t *ta_extra_or_secret(const json_t *extra, const json_t *env, const char *key) {
    const json_t *v = env ? json_obj_get(env, key) : NULL;
    if (v && v->type == JSON_STRING && *ta_skip_space(v->str_val)) return v;
    v = extra ? json_obj_get(extra, "require_mention") : NULL;
    if (!v || v->type == JSON_NULL) return NULL;
    if (v->type == JSON_STRING && !*ta_skip_space(v->str_val)) return NULL;
    return v;
}

int w29_adapter_1c52a4_p1_require_mention(const json_t *extra, const json_t *env) {
    const json_t *configured = ta_extra_or_secret(extra, env, "TEAMS_REQUIRE_MENTION");
    char *s;
    int truthy;
    if (!configured) return 0;
    if (configured->type == JSON_BOOL) return configured->bool_val ? 1 : 0;
    s = ta_trimmed_copy(ta_scalar_str(configured));
    if (!s) return 0;
    {
        char *p;
        for (p = s; *p; p++)
            if (*p >= 'A' && *p <= 'Z') *p = (char)(*p - 'A' + 'a');
    }
    truthy = !(!strcmp(s, "false") || !strcmp(s, "0") || !strcmp(s, "no") || !strcmp(s, "off"));
    free(s);
    return truthy;
}

json_t *w29_adapter_1c52a4_p1_card_action_denied(const json_t *env,
                                                 const char *aad_object_id,
                                                 const char *user_id,
                                                 const char *deny_text,
                                                 const char *unauth_text) {
    const char *allow_all = env ? json_get_str(env, "TEAMS_ALLOW_ALL_USERS", "") : "";
    char *csv, *walk, *clicker;
    const char *allow = user_id ? user_id : "";
    int allowed = 0;
    char *aa = ta_trimmed_copy(allow_all);
    if (aa) {
        char *p;
        for (p = aa; *p; p++)
            if (*p >= 'A' && *p <= 'Z') *p = (char)(*p - 'A' + 'a');
        if (!strcmp(aa, "1") || !strcmp(aa, "true") || !strcmp(aa, "yes")) {
            free(aa);
            return json_null();
        }
        free(aa);
    }
    csv = ta_trimmed_copy(env ? json_get_str(env, "TEAMS_ALLOWED_USERS", "") : "");
    if (!csv || !*csv) {
        free(csv);
        return json_string(deny_text ? deny_text : "");
    }
    clicker = ta_dup((aad_object_id && *aad_object_id) ? aad_object_id : allow);
    /* {uid.strip() for uid in csv.split(",") if uid.strip()} */
    walk = csv;
    while (walk && !allowed) {
        char *comma = strchr(walk, ',');
        const char *end = comma ? comma : walk + strlen(walk);
        char *item = ta_dup_range(walk, end);
        char *trimmed = item ? ta_trimmed_copy(item) : NULL;
        if (trimmed && *trimmed && (!strcmp(trimmed, "*") || !strcmp(trimmed, clicker)))
            allowed = 1;
        free(trimmed);
        free(item);
        walk = comma ? comma + 1 : NULL;
    }
    free(clicker);
    free(csv);
    if (!allowed) return json_string(unauth_text ? unauth_text : "");
    return json_null();
}

json_t *w29_adapter_1c52a4_p1_constants(void) {
    json_t *o = json_object();
    json_t *hosts = json_array();
    json_t *chat = json_object();
    json_t *media = json_array();
    json_t *choices = json_object();
    json_t *labels = json_object();
    json_t *ea_actions = json_object();
    json_t *ea_styles = json_object();
    size_t i;
    for (i = 0; i < TA_ALLOWED_HOSTS_N; i++) json_append(hosts, json_string(TA_ALLOWED_HOSTS[i]));
    json_set(o, "default_port", json_number(3978));
    json_set(o, "max_body_bytes", json_number(1048576));
    json_set(o, "webhook_path", json_string("/api/messages"));
    json_set(o, "default_service_url", json_string("https://smba.trafficmanager.net/teams/"));
    json_set(o, "allowed_hosts", hosts);
    json_set(o, "bf_token_scope", json_string("https://api.botframework.com/.default"));
    json_set(chat, "personal", json_string("dm"));
    json_set(chat, "groupChat", json_string("group"));
    json_set(chat, "channel", json_string("channel"));
    json_set(o, "chat_types", chat);
    {
        static const char *const kinds[4] = {"document", "image", "video", "audio"};
        static const char *const vals[4]  = {"document", "photo", "video", "audio"};
        for (i = 0; i < 4; i++) {
            json_t *pair = json_array();
            json_append(pair, json_string(kinds[i]));
            json_append(pair, json_string(vals[i]));
            json_append(media, pair);
        }
    }
    json_set(o, "media_precedence", media);
    json_set(choices, "approve_once", json_string("once"));
    json_set(choices, "approve_session", json_string("session"));
    json_set(choices, "approve_always", json_string("always"));
    json_set(choices, "deny", json_string("deny"));
    json_set(o, "approval_choices", choices);
    json_set(labels, "once", json_string("platform.teams.approval.resolved_once"));
    json_set(labels, "session", json_string("platform.teams.approval.resolved_session"));
    json_set(labels, "always", json_string("platform.teams.approval.resolved_always"));
    json_set(labels, "deny", json_string("platform.teams.approval.resolved_deny"));
    json_set(o, "approval_label_keys", labels);
    json_set(o, "max_message_length", json_number(28000));
    json_set(o, "splits_long_messages", json_bool(1));
    json_set(o, "ea_cmd_budget", json_number(2000));
    json_set(o, "ea_previews_kept", json_number(256));
    json_set(ea_actions, "once", json_string("approve_once"));
    json_set(ea_actions, "session", json_string("approve_session"));
    json_set(ea_actions, "always", json_string("approve_always"));
    json_set(ea_actions, "deny", json_string("deny"));
    json_set(o, "ea_card_actions", ea_actions);
    json_set(ea_styles, "primary", json_string("positive"));
    json_set(ea_styles, "danger", json_string("destructive"));
    json_set(o, "ea_card_styles", ea_styles);
    return o;
}

/* ---------------------------------------------------------------- p2 ---- */

/* str(getattr(getattr(e, "mentioned", None), "id", "")) */
static char *ta_mention_id(const json_t *entity) {
    const json_t *m = json_obj_get(entity, "mentioned");
    const json_t *id;
    if (!m || m->type != JSON_OBJECT) return ta_dup("");
    id = json_obj_get(m, "id");
    if (!id) return ta_dup("");
    if (id->type == JSON_STRING) return ta_dup(id->str_val);
    if (id->type == JSON_NUMBER) return ta_scalar_str(id);
    if (id->type == JSON_BOOL) return ta_dup(id->bool_val ? "True" : "False");
    return ta_dup("");
}

static int ta_in_bot_ids(const json_t *bot_ids, const char *s) {
    size_t i, n = json_len(bot_ids);
    for (i = 0; i < n; i++) {
        const json_t *v = json_get(bot_ids, i);
        if (v && v->type == JSON_STRING && !strcmp(v->str_val ? v->str_val : "", s)) return 1;
    }
    return 0;
}

int w29_adapter_1c52a4_p2_mentions_bot(const json_t *entities,
                                       const char *text,
                                       const json_t *bot_ids) {
    size_t i, n = json_len(entities);
    int mentions = 0, hit = 0;
    for (i = 0; i < n; i++) {
        const json_t *e = json_get(entities, i);
        const char *type;
        if (!e || e->type != JSON_OBJECT) continue;
        type = json_get_str(e, "type", NULL);
        if (!type || strcmp(type, "mention") != 0) continue;
        mentions++;
        if (!hit) {
            char *mid = ta_mention_id(e);
            if (mid && ta_in_bot_ids(bot_ids, mid)) hit = 1;
            free(mid);
        }
    }
    if (!mentions) return text && strstr(text, "<at>") != NULL;
    return hit;
}

json_t *w29_adapter_1c52a4_p2_approval_body(const char *cmd, const char *desc,
                                            int always, const char *header,
                                            const char *reason_label) {
    json_t *body = json_array();
    if ((cmd && *cmd) || always) {
        json_t *block = json_object();
        size_t n = strlen(header ? header : "") + 8;
        char *t = (char *)malloc(n);
        if (!t) return body;
        snprintf(t, n, "\u26a0\ufe0f %s", header ? header : "");
        json_set(block, "text", json_string(t));
        free(t);
        json_set(block, "wrap", json_bool(1));
        json_set(block, "weight", json_string("Bolder"));
        json_append(body, block);
        n = strlen(cmd ? cmd : "") + 12;
        t = (char *)malloc(n);
        if (!t) return body;
        snprintf(t, n, "```\n%s\n```", cmd ? cmd : "");
        block = json_object();
        json_set(block, "text", json_string(t));
        free(t);
        json_set(block, "wrap", json_bool(1));
        json_append(body, block);
    }
    if ((desc && *desc) || always) {
        json_t *block = json_object();
        size_t n = strlen(reason_label ? reason_label : "") + strlen(desc ? desc : "") + 3;
        char *t = (char *)malloc(n);
        if (!t) return body;
        snprintf(t, n, "%s: %s", reason_label ? reason_label : "", desc ? desc : "");
        json_set(block, "text", json_string(t));
        free(t);
        json_set(block, "wrap", json_bool(1));
        json_set(block, "isSubtle", json_bool(1));
        json_append(body, block);
    }
    return body;
}

json_t *w29_adapter_1c52a4_p2_media_type(const json_t *kinds) {
    static const char *const order[4] = {"document", "image", "video", "audio"};
    static const char *const value[4] = {"document", "photo", "video", "audio"};
    size_t i, j, n = json_len(kinds);
    for (i = 0; i < 4; i++) {
        for (j = 0; j < n; j++) {
            const json_t *k = json_get(kinds, j);
            if (k && k->type == JSON_STRING && !strcmp(k->str_val ? k->str_val : "", order[i]))
                return json_string(value[i]);
        }
    }
    return json_string("text");
}

int w29_adapter_1c52a4_p2_attachment_skip(const char *content_type,
                                          const char *content_url) {
    char *ct = ta_dup(content_type ? content_type : "");
    int skip;
    char *p;
    if (!ct) return 0;
    for (p = ct; *p; p++)
        if (*p >= 'A' && *p <= 'Z') *p = (char)(*p - 'A' + 'a');
    skip = ((!strcmp(ct, "text/html") || !strcmp(ct, "text/plain")) &&
            !(content_url && *content_url)) ||
           !strncmp(ct, "application/vnd.microsoft.card", 30);
    free(ct);
    return skip;
}

static char *ta_lstrip_dots(const char *s) {
    const char *b = s ? s : "";
    while (*b == '.') b++;
    return ta_dup(b);
}

json_t *w29_adapter_1c52a4_p2_document_filename(const char *name,
                                                const json_t *content) {
    const json_t *ft = content ? json_obj_get(content, "fileType") : NULL;
    char *raw, *clean, *out;
    size_t n;
    if (name && *name) return json_string(name);
    if (!ta_truthy(ft) && content) ft = json_obj_get(content, "file_type");
    if (!ta_truthy(ft)) return json_string("document");
    raw = ta_scalar_str(ft);
    if (!raw) return json_string("document");
    clean = ta_lstrip_dots(raw);
    free(raw);
    if (!clean || !*clean) { free(clean); return json_string("document"); }
    n = strlen(clean) + 10;
    out = (char *)malloc(n);
    if (!out) { free(clean); return json_string("document"); }
    snprintf(out, n, "document.%s", clean);
    free(clean);
    { json_t *v = json_string(out); free(out); return v; }
}

int w29_adapter_1c52a4_p2_threaded_reply(const char *reply_to) {
    const char *p;
    if (!reply_to || !*reply_to) return 0;
    for (p = reply_to; *p; p++)
        if (*p < '0' || *p > '9') return 0;
    return strcmp(reply_to, "0") != 0;
}
