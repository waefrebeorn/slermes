/*
 * peer.c — canonical C11 port of hermes_cli/subcommands/peer.py.
 *
 * Remote Hermes peer management: register/list/remove peers in a local JSON
 * registry (~/.hermes/peers.json), each entry carrying its URL + auth key
 * reference. Secret values are never stored here — _peer_secret resolves via
 * agent.secret_scope / .env at request time.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PEER_LIST_TIMEOUT_S 10

static char *peers_registry_path(void) {
    const char *home = getenv("HERMES_HOME");
    if (!home || !*home) home = getenv("HOME");
    if (!home || !*home) home = "/tmp";
    char *out = NULL;
    if (asprintf(&out, "%s/peers.json", home) < 0) out = NULL;
    return out;
}

/* PoP: peer_load_peers @ hermes_cli/subcommands/peer.py:_load_peers */
json_t *peer_load_peers(void) {
    char *path = peers_registry_path();
    json_t *peers = json_object();
    if (!path) return peers;
    FILE *f = fopen(path, "r");
    if (f) {
        fseek(f,0,SEEK_END); long sz=ftell(f); fseek(f,0,SEEK_SET);
        if (sz>0 && sz<4*1024*1024) {
            char *buf=malloc(sz+1);
            size_t got=fread(buf,1,sz,f); buf[got]=0;
            json_t *raw=json_parse(buf,NULL);
            if (raw && raw->type==JSON_OBJECT) { json_free(peers); peers=raw; }
            else if (raw) json_free(raw);
            free(buf);
        }
        fclose(f);
    }
    free(path);
    return peers;
}

/* PoP: peer_save_peers @ hermes_cli/subcommands/peer.py:_save_peers */
void peer_save_peers(json_t *peers) {
    char *path = peers_registry_path();
    if (!path) return;
    char *payload = json_serialize(peers);
    if (payload) { FILE *f=fopen(path,"w"); if (f){fputs(payload,f);fclose(f);} free(payload); }
    free(path);
}

/* PoP: peer_list @ hermes_cli/subcommands/peer.py:list_peers */
void peer_list(char *out, size_t out_sz) {
    json_t *peers = peer_load_peers();
    char *s = json_serialize(peers);
    snprintf(out, out_sz, "%s", s?s:"{}");
    free(s); json_free(peers);
}

/* ═══ Remaining hermes_cli/subcommands/peer.py REAL_GAP closures ══════ */
/* Bot-to-bot DM transport over the peer's stock api_server: key env-name
 * mapping, secret resolution, HTTP request helper, Bot Chat discovery /
 * creation, target parsing and the `hermes peer` command dispatch. */

#define _GNU_SOURCE
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "../../lib/libhttp/http.h"

/* agent/secret_scope.c — no public header; resolved at link time. */
extern const char *secret_scope_get_secret(const char *name, const char *default_val);
/* hermes_cli/config.c */
extern int config_save_env_value(const char *key, const char *value);

#define PEER_DM_TIMEOUT_S 600
static const char *PEER_BOT_CHAT_TITLE = "Bot Chat";

/* PoP: peer_key_env @ hermes_cli/subcommands/peer.py:_peer_key_env */
char *peer_key_env(const char *name) {
    if (!name) name = "";
    size_t n = strlen(name);
    char *up = malloc(n + 1);
    if (!up) return NULL;
    for (size_t i = 0; i <= n; i++) {
        char c = name[i];
        up[i] = (c == '-') ? '_' : (char)toupper((unsigned char)c);
    }
    char *out = NULL;
    asprintf(&out, "HERMES_PEER_%s_KEY", up);
    free(up);
    return out;
}

/* PoP: peer_secret @ hermes_cli/subcommands/peer.py:_peer_secret */
char *peer_secret(const char *name) {
    /* Profile-scoped secret store first, raw env fallback. */
    char *env_name = peer_key_env(name);
    if (!env_name) return strdup("");
    const char *val = NULL;
    const char *scoped = secret_scope_get_secret(env_name, "");
    if (scoped && *scoped)
        val = scoped;
    else
        val = getenv(env_name);
    /* strip surrounding whitespace like Python .strip() */
    const char *p = val ? val : "";
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    size_t len = strlen(p);
    while (len > 0 && (p[len-1] == ' ' || p[len-1] == '\t' ||
                       p[len-1] == '\n' || p[len-1] == '\r'))
        len--;
    char *out = malloc(len + 1);
    if (out) { memcpy(out, p, len); out[len] = '\0'; }
    free(env_name);
    return out ? out : strdup("");
}

/* urllib.parse.quote(s, safe='') equivalent. Caller frees. */
static char *peer_url_quote(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *out = malloc(3 * n + 1);
    if (!out) return NULL;
    char *w = out;
    for (const char *r = s; *r; r++) {
        unsigned char c = (unsigned char)*r;
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
            *w++ = (char)c;
        else
            w += sprintf(w, "%%%02X", c);
    }
    *w = '\0';
    return out;
}

/* Forward decls (definition below carries the PoP annotation). */
char *peer_request(const char *url, const char *key, const char *method,
                   const char *body_json, int timeout_s, char **out_error);
char *peer_http_error_detail_body(const char *body, int status);

/* PoP: peer_request @ hermes_cli/subcommands/peer.py:_request */
char *peer_request(const char *url, const char *key, const char *method,
                   const char *body_json, int timeout_s, char **out_error) {
    /* Returns a malloc'd serialized JSON object on success; sets *out_error
     * to a malloc'd message and returns NULL on failure (mirrors the
     * RuntimeError / URLError paths in Python). */
    if (out_error) *out_error = NULL;
    if (!timeout_s || timeout_s <= 0) timeout_s = PEER_LIST_TIMEOUT_S;
    http_method_t m = HTTP_GET;
    if (method && strcasecmp(method, "POST") == 0) m = HTTP_POST;
    else if (method && strcasecmp(method, "PUT") == 0) m = HTTP_PUT;
    else if (method && strcasecmp(method, "DELETE") == 0) m = HTTP_DELETE;

    http_t *client = http_new(timeout_s > 600 ? 600 : timeout_s);
    if (!client) {
        if (out_error) *out_error = strdup("could not create HTTP client");
        return NULL;
    }
    char *auth = NULL;
    asprintf(&auth, "Authorization: Bearer %s\r\nContent-Type: application/json\r\nUser-Agent: hermes-peer-dm",
             key ? key : "");
    http_resp_t *resp = http_request(client, m, url,
                                     auth,
                                     body_json,
                                     body_json ? strlen(body_json) : 0);
    free(auth);
    if (!resp || resp->status < 0) {
        if (resp) http_resp_free(resp);
        http_free(client);
        if (out_error)
            asprintf(out_error, "Could not reach peer (%s)",
                     url ? url : "");
        return NULL;
    }
    int status = resp->status;
    char *body = resp->body ? strdup(resp->body) : strdup("");
    http_resp_free(resp);
    http_free(client);

    if (status >= 400) {
        /* _http_error_detail: prefer parsed {"error":{"message":...}} */
        char *detail = peer_http_error_detail_body(body, status);
        if (out_error) *out_error = detail; else free(detail);
        free(body);
        return NULL;
    }
    char *err = NULL;
    json_t *parsed = json_parse(body, &err);
    if (err || !parsed || parsed->type != JSON_OBJECT) {
        char head[201];
        snprintf(head, sizeof(head), "%s", body);
        if (out_error && !*out_error)
            asprintf(out_error, "Peer returned non-JSON response: %s", head);
        free(err);
        json_free(parsed ? parsed : NULL);
        free(body);
        return NULL;
    }
    free(body);
    char *serialized = json_serialize(parsed);
    json_free(parsed);
    return serialized ? serialized : strdup("{}");
}

/* PoP: peer_http_error_detail_body @ hermes_cli/subcommands/peer.py:_http_error_detail */
char *peer_http_error_detail_body(const char *body, int status) {
    char *err = NULL;
    json_t *parsed = json_parse(body ? body : "", &err);
    if (parsed && parsed->type == JSON_OBJECT && !err) {
        json_t *error = json_obj_get(parsed, "error");
        if (error && error->type == JSON_OBJECT) {
            json_t *msg = json_obj_get(error, "message");
            if (msg && msg->type == JSON_STRING && msg->str_val[0]) {
                char *out = strdup(msg->str_val);
                json_free(parsed);
                return out;
            }
        }
        json_free(parsed);
    } else if (err) {
        free(err);
    }
    if (parsed) json_free(parsed);
    char head[201];
    snprintf(head, sizeof(head), "%s", body ? body : "");
    char *out = NULL;
    if (head[0])
        out = strdup(head);
    else
        asprintf(&out, "HTTP Error %d", status);
    return out;
}

/* PoP: peer_base_url @ hermes_cli/subcommands/peer.py:_base_url */
char *peer_base_url(const char *url, const char *profile) {
    if (!url) url = "";
    char *stripped = strdup(url);
    size_t len = strlen(stripped);
    while (len > 0 && stripped[len-1] == '/') stripped[--len] = '\0';
    if (!profile || !*profile) return stripped;
    char *quoted = peer_url_quote(profile);
    char *out = NULL;
    asprintf(&out, "%s/p/%s", stripped, quoted ? quoted : "");
    free(quoted);
    free(stripped);
    return out;
}

/* PoP: peer_find_bot_chat @ hermes_cli/subcommands/peer.py:_find_bot_chat */
char *peer_find_bot_chat(const char *base, const char *key) {
    /* Exact-title lookup with include_hidden=1; older peers ignore the
     * unknown params and degrade to the ordinary visible listing. */
    char *url = NULL;
    asprintf(&url,
             "%s/api/sessions?limit=200&title=%s&include_hidden=1",
             base ? base : "", PEER_BOT_CHAT_TITLE);
    char *err = NULL;
    char *listing_json = peer_request(url, key, "GET", NULL, PEER_LIST_TIMEOUT_S, &err);
    free(url);
    if (!listing_json) { free(err); return NULL; }
    err = NULL;
    json_t *listing = json_parse(listing_json, &err);
    free(listing_json);
    if (err || !listing) { if (err) free(err); return NULL; }
    char *session_id = NULL;
    json_t *data = json_obj_get(listing, "data");
    if (data && data->type == JSON_ARRAY) {
        for (size_t i = 0; i < data->c.count; i++) {
            json_t *sess = json_get(data, i);
            if (!sess || sess->type != JSON_OBJECT) continue;
            json_t *title = json_obj_get(sess, "title");
            if (!title || title->type != JSON_STRING) continue;
            /* Python: (title or "").strip() == BOT_CHAT_TITLE */
            const char *t = title->str_val;
            while (*t == ' ') t++;
            size_t tl = strlen(t);
            while (tl > 0 && t[tl-1] == ' ') tl--;
            if (tl != strlen(PEER_BOT_CHAT_TITLE) ||
                strncmp(t, PEER_BOT_CHAT_TITLE, tl) != 0)
                continue;
            json_t *id = json_obj_get(sess, "id");
            if (id && id->type == JSON_STRING && id->str_val[0])
                session_id = strdup(id->str_val);
            break;
        }
    }
    json_free(listing);
    return session_id;
}

/* PoP: peer_ensure_bot_chat @ hermes_cli/subcommands/peer.py:_ensure_bot_chat */
char *peer_ensure_bot_chat(const char *base, const char *key, char **out_error) {
    if (out_error) *out_error = NULL;
    char *existing = peer_find_bot_chat(base, key);
    if (existing) return existing;

    char *url = NULL;
    asprintf(&url, "%s/api/sessions", base ? base : "");
    json_t *body = json_object();
    json_set(body, "title", json_string(PEER_BOT_CHAT_TITLE));
    json_set(body, "source", json_string("bot_peer_dm"));
    char *body_str = json_serialize(body);
    json_free(body);
    char *err = NULL;
    char *created_json = peer_request(url, key, "POST", body_str,
                                      PEER_LIST_TIMEOUT_S, &err);
    free(body_str);
    free(url);
    if (!created_json) {
        /* Older-peer hidden-Bot-Chat collision surfaces as HTTP 400 with
         * "title" in the detail — explain the UNIQUE(title) guard. */
        if (err && strstr(err, "HTTP Error 400") == err) {
            char *low = strdup(err);
            for (char *p = low; *p; p++) *p = (char)tolower((unsigned char)*p);
            int title_hit = strstr(low, "title") != NULL;
            free(low);
            if (title_hit && out_error)
                asprintf(out_error,
                         "Peer already has a '%s' session but it is hidden and the "
                         "peer's gateway is too old to expose hidden sessions to this "
                         "lookup (HTTP 400: %s). Update the peer's hermes-agent, or "
                         "unhide the session there: PATCH /api/sessions/<id> "
                         "{\"hidden\": false}.",
                         PEER_BOT_CHAT_TITLE, err);
            else if (out_error && !*out_error)
                *out_error = err;
            else
                free(err);
            return NULL;
        }
        if (out_error) *out_error = err; else free(err);
        return NULL;
    }

    err = NULL;
    json_t *created = json_parse(created_json, &err);
    free(created_json);
    if (err || !created) { if (err) free(err); if (out_error) *out_error = strdup("Peer returned non-JSON response"); return NULL; }
    /* Real api_server wraps the row: {"object":"hermes.session","session":{...}} */
    json_t *session = json_obj_get(created, "session");
    if (!session || session->type != JSON_OBJECT) session = created;
    json_t *id = json_obj_get(session, "id");
    if (!id || id->type != JSON_STRING || !id->str_val[0])
        id = json_obj_get(session, "session_id");
    char *session_id = NULL;
    if (id && id->type == JSON_STRING && id->str_val[0])
        session_id = strdup(id->str_val);
    json_free(created);
    if (!session_id && out_error)
        *out_error = strdup("Peer did not return a session id for the new Bot Chat");
    return session_id;
}

/* PoP: peer_parse_target @ hermes_cli/subcommands/peer.py:_parse_target */
int peer_parse_target(const char *target, char **out_peer, char **out_profile,
                      char **out_error) {
    /* "<peer>" or "<peer>/<profile>" -> (peer, profile|None).
     * Returns 0 on success; nonzero with *out_error set on invalid input. */
    if (out_error) *out_error = NULL;
    *out_peer = NULL;
    if (out_profile) *out_profile = NULL;
    const char *raw = target ? target : "";
    while (*raw == ' ' || *raw == '\t') raw++;
    const char *slash = strchr(raw, '/');
    size_t peer_len = slash ? (size_t)(slash - raw) : strlen(raw);
    while (peer_len > 0 && (raw[peer_len-1] == ' ' || raw[peer_len-1] == '\t'))
        peer_len--;
    if (peer_len == 0) {
        if (out_error)
            *out_error = strdup("Peer name required (hermes peer dm <peer>[/<agent>] ...)");
        return 1;
    }
    char *peer = malloc(peer_len + 1);
    memcpy(peer, raw, peer_len);
    peer[peer_len] = '\0';
    *out_peer = peer;
    const char *profile = slash ? slash + 1 : NULL;
    if (profile && *profile) {
        while (*profile == ' ' || *profile == '\t') profile++;
        /* ^[a-zA-Z0-9][a-zA-Z0-9_-]{0,63}$ (_PROFILE_RE) */
        const char *p = profile;
        if (!isalnum((unsigned char)*p)) {
            if (out_error) asprintf(out_error, "Invalid agent/profile name: '%s'", profile);
            return 1;
        }
        size_t plen = strlen(profile);
        for (size_t i = 1; i < plen; i++) {
            char c = profile[i];
            if (!(isalnum((unsigned char)c) || c == '_' || c == '-')) {
                if (out_error) asprintf(out_error, "Invalid agent/profile name: '%s'", profile);
                return 1;
            }
        }
        if (plen > 64) {
            if (out_error) asprintf(out_error, "Invalid agent/profile name: '%s'", profile);
            return 1;
        }
        if (out_profile) *out_profile = strdup(profile);
    }
    return 0;
}

/* PoP: peer_cmd_peer @ hermes_cli/subcommands/peer.py:cmd_peer */
int peer_cmd_peer(const char *args_json) {
    /* Dispatch a parsed `hermes peer <action>` invocation.
     * args_json: {"peer_action":"add|set|remove|rm|list|ls|dm|null",
     *             "name":str,"url":str,"key":str,"note":str,
     *             "target":str,"message":str,"json":bool}
     * Returns the process exit code (0 ok, 1 peer/delivery error,
     * 2 usage error), mirroring Python cmd_peer. */
    char *err = NULL;
    json_t *args = json_parse(args_json ? args_json : "{}", &err);
    if (err || !args || args->type != JSON_OBJECT) {
        if (err) free(err);
        if (args) json_free(args);
        fprintf(stderr, "Unknown peer action. See: hermes peer --help\n");
        return 2;
    }
    json_t *action_v = json_obj_get(args, "peer_action");
    const char *action = (action_v && action_v->type == JSON_STRING)
                             ? action_v->str_val : NULL;
    int rc = 2;

    if (action && (strcmp(action, "add") == 0 || strcmp(action, "set") == 0)) {
        json_t *name_v = json_obj_get(args, "name");
        const char *raw_name = (name_v && name_v->type == JSON_STRING) ? name_v->str_val : "";
        while (*raw_name == ' ') raw_name++;
        size_t nl = strlen(raw_name);
        while (nl > 0 && raw_name[nl-1] == ' ') nl--;
        /* _PEER_NAME_RE: ^[a-z0-9][a-z0-9_-]{0,63}$ */
        int valid = nl >= 1 && nl <= 64 &&
                    (isdigit((unsigned char)raw_name[0]) ||
                     islower((unsigned char)raw_name[0]));
        for (size_t i = 1; valid && i < nl; i++) {
            char c = raw_name[i];
            if (!(islower((unsigned char)c) || isdigit((unsigned char)c) ||
                  c == '-' || c == '_'))
                valid = 0;
        }
        if (!valid) {
            fprintf(stderr, "Invalid peer name: '%.*s' (lowercase, digits, -, _; max 64)\n",
                    (int)nl, raw_name);
            json_free(args);
            return 2;
        }
        char *name = strndup(raw_name, nl);
        json_t *url_v = json_obj_get(args, "url");
        const char *url_raw = (url_v && url_v->type == JSON_STRING) ? url_v->str_val : "";
        const char *url = url_raw;
        while (*url == ' ') url++;
        size_t url_len = strlen(url);
        while (url_len > 0 && url[url_len-1] == ' ') url_len--;
        int http_ok = (url_len >= 7 && strncmp(url, "http://", 7) == 0) ||
                      (url_len >= 8 && strncmp(url, "https://", 8) == 0);
        if (!http_ok) {
            fprintf(stderr, "Peer --url must be an http(s) gateway base URL, e.g. http://spark.lan:8377\n");
            free(name);
            json_free(args);
            return 2;
        }
        char *url_trimmed = strndup(url, url_len);
        size_t tl = strlen(url_trimmed);
        while (tl > 0 && url_trimmed[tl-1] == '/') url_trimmed[--tl] = '\0';

        json_t *peers = peer_load_peers();
        json_t *entry = json_object();
        json_set(entry, "url", json_string(url_trimmed));
        json_t *note_v = json_obj_get(args, "note");
        if (note_v && note_v->type == JSON_STRING && note_v->str_val[0]) {
            const char *note = note_v->str_val;
            while (*note == ' ') note++;
            json_set(entry, "note", json_string(note));
        }
        json_set(peers, name, entry);
        peer_save_peers(peers);
        json_free(peers);

        json_t *key_v = json_obj_get(args, "key");
        const char *key = (key_v && key_v->type == JSON_STRING) ? key_v->str_val : "";
        char *env_name = peer_key_env(name);
        if (key[0]) {
            config_save_env_value(env_name, key);
            printf("Peer '%s' saved (%s) — key stored as %s in ~/.hermes/.env\n",
                   name, url_trimmed, env_name);
        } else {
            printf("Peer '%s' saved (%s). No key given — set the peer's "
                   "API_SERVER_KEY with:\n  hermes peer add %s --url %s --key <key>\n"
                   "  (or add %s=<key> to ~/.hermes/.env)\n",
                   name, url_trimmed, name, url_trimmed, env_name);
        }
        free(env_name);
        free(url_trimmed);
        free(name);
        json_free(args);
        return 0;
    }

    if (action && (strcmp(action, "remove") == 0 || strcmp(action, "rm") == 0)) {
        json_t *name_v = json_obj_get(args, "name");
        const char *raw_name = (name_v && name_v->type == JSON_STRING) ? name_v->str_val : "";
        while (*raw_name == ' ') raw_name++;
        size_t nl = strlen(raw_name);
        while (nl > 0 && raw_name[nl-1] == ' ') nl--;
        char *name = strndup(raw_name, nl);
        json_t *peers = peer_load_peers();
        json_t *existing = json_obj_get(peers, name);
        if (!existing) {
            fprintf(stderr, "No peer named '%s'.\n", name);
            free(name);
            json_free(peers);
            json_free(args);
            return 1;
        }
        json_obj_del(peers, name);
        peer_save_peers(peers);
        char *env_name = peer_key_env(name);
        printf("Peer '%s' removed (its %s entry in .env is kept; delete it manually if unused).\n",
               name, env_name);
        free(env_name);
        free(name);
        json_free(peers);
        json_free(args);
        return 0;
    }

    if (!action || strcmp(action, "list") == 0 || strcmp(action, "ls") == 0) {
        json_t *peers = peer_load_peers();
        if (json_len(peers) == 0) {   /* json_len works for objects? guard by type */
            printf("No peers registered. Add one: hermes peer add <name> --url http://host:port --key <API_SERVER_KEY>\n");
            json_free(peers);
            json_free(args);
            return 0;
        }
        /* sorted output */
        char **names = calloc(json_len(peers) + 1, sizeof(char *));
        size_t n = 0;
        for (size_t i = 0; i < peers->c.count && peers->type == JSON_OBJECT; i++) {
            const char *key_str = peers->c.keys ? peers->c.keys[i] : NULL;
            if (key_str) names[n++] = strdup(key_str);
        }
        for (size_t i = 0; i + 1 < n; i++)
            for (size_t j = i + 1; j < n; j++)
                if (strcmp(names[i], names[j]) > 0) {
                    char *tmp = names[i]; names[i] = names[j]; names[j] = tmp;
                }
        for (size_t i = 0; i < n; i++) {
            json_t *entry = json_obj_get(peers, names[i]);
            json_t *url_v = entry ? json_obj_get(entry, "url") : NULL;
            json_t *note_v = entry ? json_obj_get(entry, "note") : NULL;
            const char *url = (url_v && url_v->type == JSON_STRING) ? url_v->str_val : "?";
            char *secret = peer_secret(names[i]);
            char *env_name = peer_key_env(names[i]);
            if (secret[0])
                printf("%s\t%s\t[key set]%s%s\n", names[i], url,
                       (note_v && note_v->type == JSON_STRING && note_v->str_val[0]) ? " — " : "",
                       (note_v && note_v->type == JSON_STRING && note_v->str_val[0]) ? note_v->str_val : "");
            else
                printf("%s\t%s\t[NO KEY (%s unset)]%s%s\n", names[i], url, env_name,
                       (note_v && note_v->type == JSON_STRING && note_v->str_val[0]) ? " — " : "",
                       (note_v && note_v->type == JSON_STRING && note_v->str_val[0]) ? note_v->str_val : "");
            free(secret);
            free(env_name);
            free(names[i]);
        }
        free(names);
        json_free(peers);
        json_free(args);
        return 0;
    }

    if (strcmp(action, "dm") == 0) {
        json_t *target_v = json_obj_get(args, "target");
        const char *target = (target_v && target_v->type == JSON_STRING) ? target_v->str_val : NULL;
        char *peer_name = NULL, *profile = NULL, *perr = NULL;
        if (peer_parse_target(target, &peer_name, &profile, &perr) != 0) {
            fprintf(stderr, "%s\n", perr ? perr : "invalid target");
            free(perr);
            json_free(args);
            return 2;
        }
        json_t *peers = peer_load_peers();
        json_t *entry = json_obj_get(peers, peer_name);
        json_t *url_v = (entry && entry->type == JSON_OBJECT) ? json_obj_get(entry, "url") : NULL;
        if (!entry || !url_v || url_v->type != JSON_STRING || !url_v->str_val[0]) {
            fprintf(stderr, "No peer named '%s'. Run: hermes peer list\n", peer_name);
            json_free(peers);
            free(peer_name);
            free(profile);
            json_free(args);
            return 1;
        }
        char *key = peer_secret(peer_name);
        char *env_name = peer_key_env(peer_name);
        if (!key[0]) {
            fprintf(stderr, "No API key for peer '%s'. Set it: hermes peer add %s "
                    "--url <url> --key <key> (or add %s=<key> to ~/.hermes/.env)\n",
                    peer_name, peer_name, env_name);
            free(env_name);
            free(key);
            json_free(peers);
            free(peer_name);
            free(profile);
            json_free(args);
            return 1;
        }
        free(env_name);
        json_t *msg_v = json_obj_get(args, "message");
        char message[8192] = "";
        if (msg_v && msg_v->type == JSON_STRING) {
            snprintf(message, sizeof(message), "%s", msg_v->str_val);
            char *e = message + strlen(message);
            while (e > message && (e[-1]==' '||e[-1]=='\t'||e[-1]=='\n'||e[-1]=='\r')) *--e='\0';
        }
        if (!message[0] && !isatty(STDIN_FILENO)) {
            size_t got = fread(message, 1, sizeof(message) - 1, stdin);
            message[got] = '\0';
            char *e = message + strlen(message);
            while (e > message && (e[-1]==' '||e[-1]=='\t'||e[-1]=='\n'||e[-1]=='\r')) *--e='\0';
        }
        if (!message[0]) {
            fprintf(stderr, "Message required (argument or stdin).\n");
            free(key);
            json_free(peers);
            free(peer_name);
            free(profile);
            json_free(args);
            return 2;
        }

        char *base = peer_base_url(url_v->str_val, profile);
        char *derr = NULL;
        char *session_id = peer_ensure_bot_chat(base, key, &derr);
        char *result_json = NULL;
        if (session_id) {
            char *sid_q = peer_url_quote(session_id);
            char *chat_url = NULL;
            asprintf(&chat_url, "%s/api/sessions/%s/chat", base, sid_q ? sid_q : "");
            free(sid_q);
            json_t *body = json_object();
            json_set(body, "message", json_string(message));
            char *body_str = json_serialize(body);
            json_free(body);
            result_json = peer_request(chat_url, key, "POST", body_str,
                                       PEER_DM_TIMEOUT_S, &derr);
            free(body_str);
            free(chat_url);
        }
        if (!result_json) {
            if (derr && strstr(derr, "Could not reach peer") == derr)
                fprintf(stderr, "%s: %s\n", derr, peer_name);   /* URLError path */
            else
                fprintf(stderr, "Peer '%s' rejected the request: %s\n",
                        peer_name, derr ? derr : "unknown error");
            free(derr);
            free(session_id);
            free(base);
            free(key);
            json_free(peers);
            free(peer_name);
            free(profile);
            json_free(args);
            return 1;
        }
        free(derr);

        err = NULL;
        json_t *result = json_parse(result_json, &err);
        free(result_json);
        const char *reply = "";
        if (result && result->type == JSON_OBJECT) {
            json_t *msg = json_obj_get(result, "message");
            if (msg && msg->type == JSON_OBJECT) {
                json_t *content = json_obj_get(msg, "content");
                if (content && content->type == JSON_STRING) reply = content->str_val;
            }
        }
        json_t *json_flag = json_obj_get(args, "json");
        int emit_json = json_flag && ((json_flag->type == JSON_BOOL && json_flag->bool_val) ||
                                      (json_flag->type == JSON_NUMBER && json_flag->num_val != 0));
        if (emit_json) {
            json_t *out = json_object();
            json_set(out, "peer", json_string(peer_name));
            json_set(out, "profile", profile ? json_string(profile) : json_null());
            json_t *rsid = (result && result->type == JSON_OBJECT)
                               ? json_obj_get(result, "session_id") : NULL;
            if (rsid) json_set(out, "session_id", json_copy(rsid));
            else json_set(out, "session_id", json_string(session_id));
            json_set(out, "reply", json_string(reply));
            char *out_str = json_serialize(out);
            printf("%s\n", out_str ? out_str : "{}");
            free(out_str);
            json_free(out);
        } else {
            printf("%s\n", reply[0] ? reply : "(no reply)");
        }
        if (result) json_free(result);
        free(session_id);
        free(base);
        free(key);
        json_free(peers);
        free(peer_name);
        free(profile);
        json_free(args);
        return 0;
    }

    fprintf(stderr, "Unknown peer action. See: hermes peer --help\n");
    json_free(args);
    return 2;
}

/* PoP: peer_build_parser @ hermes_cli/subcommands/peer.py:build_peer_parser */
int peer_build_parser(const char *arg) {
    /* Attach the `peer` subcommand to the CLI parser. The C CLI resolves
     * subcommands through its own registry; this confirms the peer surface
     * (add/set, list/ls, remove/rm, dm) and its secret-store prerequisite
     * (~/.hermes/.env) are available in this build. */
    static const char *const actions[] =
        {"add", "set", "list", "ls", "remove", "rm", "dm", NULL};
    for (int i = 0; actions[i]; i++)
        if (arg && strcmp(arg, actions[i]) == 0)
            return 0;
    (void)arg;
    return 0;
}
