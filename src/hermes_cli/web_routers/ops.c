/*
 * ops.c - C11 port of upstream hermes_cli/web_routers/ops.py
 * (gates w31_ops_0ed859_p1 + w31_ops_0ed859_p2).
 *
 * PoP: w31_ops_0ed859_p1_webhook_route_summary @ hermes_cli/web_routers/ops.py:_webhook_route_summary
 * PoP: w31_ops_0ed859_p1_dashboard_route_signature @ hermes_cli/web_routers/ops.py:_dashboard_route_signature
 * PoP: w31_ops_0ed859_p1_webhook_key_in @ hermes_cli/web_routers/ops.py:_webhook_key_in
 * PoP: w31_ops_0ed859_p1_webhook_name_verdict @ hermes_cli/web_routers/ops.py:create_webhook (name normalise + slug check)
 * PoP: w31_ops_0ed859_p1_webhook_deliver_verdict @ hermes_cli/web_routers/ops.py:create_webhook (deliver_only/"log" guard)
 * PoP: w31_ops_0ed859_p2_pool_entry_summary @ hermes_cli/web_routers/ops.py:_pool_entry_summary
 * PoP: w31_ops_0ed859_p2_safe_backup_upload_name @ hermes_cli/web_routers/ops.py:_safe_backup_upload_name
 * PoP: w31_ops_0ed859_p2_hook_body_fields @ hermes_cli/web_routers/ops.py:_hook_body_fields
 * PoP: w31_ops_0ed859_p2_hook_event_verdict @ hermes_cli/web_routers/ops.py:create_hook (VALID_HOOKS guard)
 * PoP: w31_ops_0ed859_p2_spawn_import_argv @ hermes_cli/web_routers/ops.py:_spawn_import
 * PoP: w31_ops_0ed859_p2_memory_reset_target @ hermes_cli/web_routers/ops.py:reset_memory
 * PoP: w31_ops_0ed859_p2_pairing_approve_target @ hermes_cli/web_routers/ops.py:approve_pairing
 * PoP: w31_ops_0ed859_p2_backup_download_verdict @ hermes_cli/web_routers/ops.py:_dashboard_backup_path
 *
 * The ops router is thin HTTP plumbing over value transforms: a subscription
 * record becomes the summary the dashboard lists, a request body becomes the
 * argv a spawned CLI action receives, a handle's flags become a refusal. Those
 * transforms are what is ported here; the seams stay in the caller --
 * subprocess spawn, the webhook JSON store, the credential pool on disk, the
 * filesystem (exists/stat), config.yaml, and the pairing store's file lock.
 * Every status code returns as data ({"ok","status","detail"}) instead of an
 * exception, so a fixture flags the outcome the route would have produced.
 */
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w31_ops_0ed859_p1.h"
#include "noc_w31_ops_0ed859_p2.h"

#define OPS_BIG  1024
#define OPS_NAME 512
#define OPS_CP   256

/* --- shared value helpers ------------------------------------------------ */

/* Python str.strip() over ASCII whitespace. */
static void ops_strip(char *s) {
    size_t n, i = 0;
    if (!s) return;
    n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) n--;
    s[n] = '\0';
    while (s[i] && isspace((unsigned char)s[i])) i++;
    if (i) memmove(s, s + i, n - i + 1);
}

static void ops_lower(char *s) {
    for (; s && *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c >= 'A' && c <= 'Z') *s = (char)(c - 'A' + 'a');
    }
}

static void ops_upper(char *s) {
    for (; s && *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c >= 'a' && c <= 'z') *s = (char)(c - 'a' + 'A');
    }
}

/* Python truthiness of a JSON value. */
static int ops_truthy(const json_t *v) {
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

/* ``d.get(key, default)`` -- the default is used only when the key is absent. */
static json_t *ops_field_or(const json_t *obj, const char *key, const char *def) {
    json_t *v = json_obj_get(obj, key);
    return v ? json_copy(v) : json_string(def);
}

/* ``d.get(key)`` -- absent stays null. */
static json_t *ops_field_or_null(const json_t *obj, const char *key) {
    json_t *v = json_obj_get(obj, key);
    return v ? json_copy(v) : json_null();
}

/* ``list(x or [])``: a falsy value is [], an array is its items, a mapping its keys. */
static json_t *ops_list_or_empty(const json_t *v) {
    json_t *out = json_array();
    size_t i, n;
    if (!v || !ops_truthy(v)) return out;
    if (v->type == JSON_ARRAY) {
        n = json_len(v);
        for (i = 0; i < n; i++) json_append(out, json_copy(json_get(v, i)));
    } else if (v->type == JSON_OBJECT) {
        n = json_object_size(v);
        for (i = 0; i < n; i++) json_append(out, json_string(json_object_get_key_at(v, i)));
    }
    return out;
}

/* The route-verdict projection: HTTPException(status_code, detail) as data. */
static json_t *ops_verdict(int ok, int status, const char *detail) {
    json_t *out = json_object();
    json_set(out, "ok", json_bool(ok ? true : false));
    json_set(out, "status", json_number((double)status));
    json_set(out, "detail", detail ? json_string(detail) : json_null());
    return out;
}

static const char *ops_str_or_empty(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}

/* --- redact_key -> agent.redact.mask_secret ------------------------------ */

static size_t ops_utf8_len(unsigned char b) {
    if (b < 0x80) return 1;
    if ((b & 0xE0) == 0xC0) return 2;
    if ((b & 0xF0) == 0xE0) return 3;
    if ((b & 0xF8) == 0xF0) return 4;
    return 1; /* invalid lead byte: one byte is one unit */
}

static unsigned ops_utf8_decode(const unsigned char *s, size_t len, size_t *used) {
    size_t n = ops_utf8_len(s[0]);
    unsigned cp = 0;
    size_t i;
    if (n > len) { *used = 1; return s[0]; }
    if (n == 1) { *used = 1; return s[0]; }
    if (n == 2) cp = (unsigned)(s[0] & 0x1F);
    else if (n == 3) cp = (unsigned)(s[0] & 0x0F);
    else cp = (unsigned)(s[0] & 0x07);
    for (i = 1; i < n; i++) {
        if ((s[i] & 0xC0) != 0x80) { *used = 1; return s[0]; }
        cp = (cp << 6) | (unsigned)(s[i] & 0x3F);
    }
    *used = n;
    return cp;
}

/* agent.redact._DISPLAY_CONTROL_RE: C0/C1 controls, bidi overrides, zero-widths. */
static int ops_is_display_control(unsigned cp) {
    if (cp <= 0x1F || cp == 0x7F) return 1;
    if (cp >= 0x80 && cp <= 0x9F) return 1;
    if (cp >= 0x200B && cp <= 0x200F) return 1;
    if (cp >= 0x202A && cp <= 0x202E) return 1;
    if (cp >= 0x2060 && cp <= 0x2064) return 1;
    return 0;
}

/* mask_secret(value, head=4, tail=4, floor=12): strip display controls, then
 * "<12 chars -> ***" else "head...tail".  Only ever called on a truthy token. */
static void ops_mask_secret(const char *value, char *out, size_t cap) {
    unsigned char clean[OPS_BIG];
    size_t off[OPS_CP], clen[OPS_CP];
    size_t ncp = 0, n = 0, pos = 0, i, head_end, bn = 0;
    const unsigned char *s = (const unsigned char *)(value ? value : "");
    size_t len = strlen((const char *)s);
    char buf[OPS_BIG];

    while (pos < len && ncp < OPS_CP && n + 4 < OPS_BIG) {
        size_t used = 0;
        unsigned cp = ops_utf8_decode(s + pos, len - pos, &used);
        if (!ops_is_display_control(cp)) {
            off[ncp] = n;
            clen[ncp] = used;
            memcpy(clean + n, s + pos, used);
            n += used;
            ncp++;
        }
        pos += used;
    }
    clean[n] = '\0';

    if (ncp == 0) {
        if (cap) out[0] = '\0';
        return;
    }
    if (ncp < 12) {
        snprintf(out, cap, "***");
        return;
    }
    head_end = off[3] + clen[3];
    for (i = 0; i < head_end && bn < sizeof(buf) - 5; i++) buf[bn++] = (char)clean[i];
    buf[bn++] = '.';
    buf[bn++] = '.';
    buf[bn++] = '.';
    for (i = off[ncp - 4]; i < n && bn < sizeof(buf) - 1; i++) buf[bn++] = (char)clean[i];
    buf[bn] = '\0';
    snprintf(out, cap, "%s", buf);
}

/* --- gate w31_ops_0ed859_p1: webhook payload shaping --------------------- */

json_t *w31_ops_0ed859_p1_webhook_route_summary(const json_t *req) {
    const json_t *route = json_obj_get(req, "route");
    const json_t *enabled;
    const char *name = json_get_str(req, "name", "");
    const char *base = json_get_str(req, "base_url", "");
    char url[OPS_BIG];
    json_t *out;
    int is_enabled = 1;

    if (!route || route->type != JSON_OBJECT) route = NULL;
    out = json_object();
    json_set(out, "name", json_string(name));
    json_set(out, "description", ops_field_or(route, "description", ""));
    json_set(out, "events", ops_list_or_empty(json_obj_get(route, "events")));
    json_set(out, "deliver", ops_field_or(route, "deliver", "log"));
    json_set(out, "deliver_only",
             json_bool(ops_truthy(json_obj_get(route, "deliver_only")) ? true : false));
    json_set(out, "prompt", ops_field_or(route, "prompt", ""));
    json_set(out, "script", ops_field_or(route, "script", ""));
    json_set(out, "skills", ops_list_or_empty(json_obj_get(route, "skills")));
    json_set(out, "created_at", ops_field_or_null(route, "created_at"));
    snprintf(url, sizeof(url), "%s/webhooks/%s", base, name);
    json_set(out, "url", json_string(url));
    json_set(out, "signature", ops_field_or_null(route, "signature"));
    json_set(out, "secret_set", json_bool(ops_truthy(json_obj_get(route, "secret")) ? true : false));
    /* Default-enabled; only the literal False turns a route off (``is not False``). */
    enabled = route ? json_obj_get(route, "enabled") : NULL;
    if (enabled && enabled->type == JSON_BOOL && !enabled->bool_val) is_enabled = 0;
    json_set(out, "enabled", json_bool(is_enabled ? true : false));
    return out;
}

json_t *w31_ops_0ed859_p1_dashboard_route_signature(const json_t *req) {
    const json_t *requested = json_obj_get(req, "requested");
    const json_t *expected = json_obj_get(req, "expected");
    const json_t *sig;
    json_t *out = json_object();

    /* requested is not None: an explicit pin always wins. */
    if (requested && requested->type != JSON_NULL) {
        json_set(out, "signature", json_copy(requested));
        return out;
    }
    /* expected is the stored record, or the MISSING sentinel (absent/null here). */
    if (!expected || expected->type != JSON_OBJECT) {
        json_set(out, "signature", json_string("generic"));
        return out;
    }
    sig = json_obj_get(expected, "signature");
    if (sig) json_set(out, "signature", json_copy(sig));
    return out;
}

json_t *w31_ops_0ed859_p1_webhook_key_in(const json_t *req) {
    const json_t *subs = json_obj_get(req, "subscriptions");
    char key[OPS_BIG];
    size_t i, n = json_object_size(subs);
    int found = 0;

    snprintf(key, sizeof(key), "%s", ops_str_or_empty(json_obj_get(req, "name")));
    ops_strip(key);
    ops_lower(key);

    for (i = 0; i < n; i++) {
        const char *k = json_object_get_key_at(subs, i);
        if (k && strcmp(k, key) == 0) { found = 1; break; }
    }
    if (!found) {
        char detail[OPS_BIG + 64];
        snprintf(detail, sizeof(detail), "No subscription named '%s'", key);
        return ops_verdict(0, 404, detail);
    }
    {
        json_t *out = ops_verdict(1, 200, NULL);
        json_set(out, "key", json_string(key));
        return out;
    }
}

/* ^[a-z0-9][a-z0-9_-]*$ (the name is already stripped and lowercased). */
static int ops_webhook_name_ok(const char *name) {
    size_t i;
    unsigned char c;
    if (!name || !name[0]) return 0;
    c = (unsigned char)name[0];
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))) return 0;
    for (i = 1; name[i]; i++) {
        c = (unsigned char)name[i];
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-') continue;
        return 0;
    }
    return 1;
}

json_t *w31_ops_0ed859_p1_webhook_name_verdict(const json_t *req) {
    char name[OPS_BIG];
    size_t i;
    json_t *out;

    snprintf(name, sizeof(name), "%s", ops_str_or_empty(json_obj_get(req, "name")));
    ops_strip(name);
    ops_lower(name);
    for (i = 0; name[i]; i++) if (name[i] == ' ') name[i] = '-';

    if (!ops_webhook_name_ok(name))
        return ops_verdict(0, 400, "Invalid name. Use lowercase alphanumeric with hyphens/underscores.");
    out = ops_verdict(1, 200, NULL);
    json_set(out, "name", json_string(name));
    return out;
}

json_t *w31_ops_0ed859_p1_webhook_deliver_verdict(const json_t *req) {
    const json_t *deliver_node = json_obj_get(req, "deliver");
    int deliver_only = ops_truthy(json_obj_get(req, "deliver_only"));
    /* The model default is "log"; only that literal needs a real target. */
    const char *deliver = deliver_node ? ops_str_or_empty(deliver_node) : "log";

    if (deliver_only && strcmp(deliver, "log") == 0)
        return ops_verdict(0, 400,
                           "Direct delivery requires a real target (telegram, discord, \xE2\x80\xA6), not 'log'.");
    return ops_verdict(1, 200, NULL);
}

/* --- gate w31_ops_0ed859_p2: ops payload / argv derivation --------------- */

json_t *w31_ops_0ed859_p2_pool_entry_summary(const json_t *req) {
    const json_t *entry = json_obj_get(req, "entry");
    const json_t *token_node;
    char preview[OPS_BIG];
    json_t *out;

    if (!entry || entry->type != JSON_OBJECT) entry = NULL;
    out = json_object();
    json_set(out, "index", json_number(json_get_num(req, "index", 1.0)));
    json_set(out, "id", ops_field_or_null(entry, "id"));
    json_set(out, "label", ops_field_or_null(entry, "label"));
    json_set(out, "auth_type", ops_field_or_null(entry, "auth_type"));
    json_set(out, "source", ops_field_or_null(entry, "source"));
    json_set(out, "priority", ops_field_or_null(entry, "priority"));
    json_set(out, "last_status", ops_field_or_null(entry, "last_status"));
    json_set(out, "request_count", ops_field_or_null(entry, "request_count"));

    /* token = entry.access_token or ""; preview = redact_key(token) if token else "" */
    token_node = entry ? json_obj_get(entry, "access_token") : NULL;
    preview[0] = '\0';
    if (token_node && token_node->type == JSON_STRING && ops_truthy(token_node))
        ops_mask_secret(token_node->str_val, preview, sizeof(preview));
    json_set(out, "token_preview", json_string(preview));
    json_set(out, "has_refresh",
             json_bool(ops_truthy(entry ? json_obj_get(entry, "refresh_token") : NULL) ? true : false));
    return out;
}

/* Path(...).name: a drive prefix is dropped, separators and "." components are
 * ignored, and the last component is the name ("" when there is none). */
static void ops_path_name(const char *path, int nt, char *out, size_t cap) {
    const char *start = path;
    size_t len = strlen(path);
    char best[OPS_NAME];
    const char *p;

    best[0] = '\0';
    if (nt && len >= 2 && path[1] == ':' &&
        ((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')))
        start = path + 2;
    p = start;
    while (*p) {
        const char *q;
        size_t clen, copy;
        while (*p && (*p == '/' || (nt && *p == '\\'))) p++;
        if (!*p) break;
        q = p;
        while (*q && *q != '/' && !(nt && *q == '\\')) q++;
        clen = (size_t)(q - p);
        if (!(clen == 1 && *p == '.')) {
            copy = clen < sizeof(best) - 1 ? clen : sizeof(best) - 1;
            memcpy(best, p, copy);
            best[copy] = '\0';
        }
        p = q;
    }
    snprintf(out, cap, "%s", best);
}

void w31_ops_0ed859_p2_safe_backup_upload_name(const char *filename, int nt, char *out, size_t cap) {
    char base[OPS_NAME];
    char name[OPS_NAME];
    const char *raw = (filename && *filename) ? filename : "backup.zip";
    size_t n = 0, i;
    int in_run = 0;

    ops_path_name(raw, nt, base, sizeof(base));
    ops_strip(base);

    /* re.sub(r"[^A-Za-z0-9._-]+", "-", name) -- a run collapses to one dash. */
    for (i = 0; base[i] && n < sizeof(name) - 1; i++) {
        unsigned char c = (unsigned char)base[i];
        int keep = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                   (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
        if (keep) {
            name[n++] = (char)c;
            in_run = 0;
        } else if (!in_run) {
            name[n++] = '-';
            in_run = 1;
        }
    }
    name[n] = '\0';

    /* .strip(".-") */
    while (n > 0 && (name[n - 1] == '.' || name[n - 1] == '-')) name[--n] = '\0';
    i = 0;
    while (name[i] == '.' || name[i] == '-') i++;
    if (i) memmove(name, name + i, n - i + 1);

    if (!name[0]) snprintf(name, sizeof(name), "backup.zip");

    /* if not name.lower().endswith(".zip"): name += ".zip" */
    {
        size_t ln = strlen(name);
        int has_zip = ln >= 4;
        if (has_zip) {
            const char *tail = name + ln - 4;
            char low[5];
            size_t k;
            for (k = 0; k < 4; k++) low[k] = (char)tolower((unsigned char)tail[k]);
            low[4] = '\0';
            if (strcmp(low, ".zip") != 0) has_zip = 0;
        }
        if (!has_zip) snprintf(name + ln, sizeof(name) - ln, ".zip");
    }
    snprintf(out, cap, "%s", name);
}

json_t *w31_ops_0ed859_p2_hook_body_fields(const json_t *req) {
    char event[OPS_BIG], command[OPS_BIG];
    json_t *out;

    snprintf(event, sizeof(event), "%s", ops_str_or_empty(json_obj_get(req, "event")));
    snprintf(command, sizeof(command), "%s", ops_str_or_empty(json_obj_get(req, "command")));
    ops_strip(event);
    ops_strip(command);

    if (!event[0] || !command[0]) return ops_verdict(0, 400, "event and command are required");
    out = ops_verdict(1, 200, NULL);
    json_set(out, "event", json_string(event));
    json_set(out, "command", json_string(command));
    return out;
}

json_t *w31_ops_0ed859_p2_hook_event_verdict(const json_t *req) {
    json_t *fields = w31_ops_0ed859_p2_hook_body_fields(req);
    const json_t *valid = json_obj_get(req, "valid_hooks");
    const char *event_text;
    size_t i, k, n, m = 0;
    const char *sorted[64];
    int allowed = 0;

    if (!json_get_bool(fields, "ok", false)) return fields;
    event_text = json_get_str(fields, "event", "");
    /* Absent or null means the registry was unavailable -> no check. An EMPTY
     * mapping is a registry that loaded with no hooks: the check still runs and
     * every event is unknown, which is what `Valid: ` (empty list) reports. */
    if (!valid || valid->type == JSON_NULL) return fields;

    n = (valid->type == JSON_ARRAY) ? json_len(valid) : 0;
    for (i = 0; i < n; i++) {
        const json_t *item = json_get(valid, i);
        if (!item || item->type != JSON_STRING || !item->str_val) continue;
        if (strcmp(item->str_val, event_text) == 0) { allowed = 1; break; }
        if (m < 64) sorted[m++] = item->str_val;
    }
    if (allowed) return fields;

    /* f"Unknown event '{event}'. Valid: {', '.join(sorted(valid_hooks))}" */
    {
        char detail[OPS_BIG];
        size_t used;
        for (i = 0; i < m; i++) {
            for (k = i + 1; k < m; k++) {
                if (strcmp(sorted[k], sorted[i]) < 0) {
                    const char *tmp = sorted[i];
                    sorted[i] = sorted[k];
                    sorted[k] = tmp;
                }
            }
        }
        used = (size_t)snprintf(detail, sizeof(detail), "Unknown event '%s'. Valid: ", event_text);
        for (i = 0; i < m && used < sizeof(detail) - 3; i++)
            used += (size_t)snprintf(detail + used, sizeof(detail) - used, "%s%s",
                                     i ? ", " : "", sorted[i]);
        json_free(fields);
        return ops_verdict(0, 400, detail);
    }
}

json_t *w31_ops_0ed859_p2_spawn_import_argv(const json_t *req) {
    int force = json_get_bool(req, "force", false) ? 1 : 0;
    int keep_host = json_get_bool(req, "keep_host", false) ? 1 : 0;
    int messaging_off = json_get_bool(req, "messaging_off", false) ? 1 : 0;
    json_t *args = json_array();

    json_append(args, json_string("import"));
    json_append(args, json_string(json_get_str(req, "archive", "")));
    if (force || keep_host) json_append(args, json_string("--force"));
    if (keep_host) json_append(args, json_string("--keep-host"));
    if (keep_host && messaging_off) json_append(args, json_string("--messaging-off"));
    return args;
}

json_t *w31_ops_0ed859_p2_memory_reset_target(const json_t *req) {
    const json_t *raw = json_obj_get(req, "target");
    char target[OPS_BIG];
    int memory_exists = json_get_bool(req, "memory_exists", false) ? 1 : 0;
    int user_exists = json_get_bool(req, "user_exists", false) ? 1 : 0;
    static const char *const FILES[2][2] = {{"MEMORY.md", "memory"}, {"USER.md", "user"}};
    json_t *deleted = json_array();
    json_t *out;
    int all, mem, usr;
    size_t i;

    /* target = (body.target or "all").strip().lower() -- "" is falsy too. */
    if (raw && raw->type == JSON_STRING && raw->str_val && raw->str_val[0])
        snprintf(target, sizeof(target), "%s", raw->str_val);
    else
        snprintf(target, sizeof(target), "all");
    ops_strip(target);
    ops_lower(target);

    all = strcmp(target, "all") == 0;
    mem = strcmp(target, "memory") == 0;
    usr = strcmp(target, "user") == 0;
    if (!all && !mem && !usr) return ops_verdict(0, 400, "target must be all, memory, or user");

    for (i = 0; i < 2; i++) {
        int selected = all || strcmp(target, FILES[i][1]) == 0;
        int exists = strcmp(FILES[i][1], "memory") == 0 ? memory_exists : user_exists;
        if (selected && exists) json_append(deleted, json_string(FILES[i][0]));
    }
    out = ops_verdict(1, 200, NULL);
    json_set(out, "target", json_string(target));
    json_set(out, "deleted", deleted);
    return out;
}

/* PairingStore.looks_like_request_id: 16 hex chars (a code is 8 uppercase). */
static int ops_looks_like_request_id(const char *value) {
    char text[OPS_BIG];
    size_t i, n;
    snprintf(text, sizeof(text), "%s", value ? value : "");
    ops_strip(text);
    n = strlen(text);
    if (n != 16) return 0;
    for (i = 0; i < n; i++) {
        char c = text[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) return 0;
    }
    return 1;
}

json_t *w31_ops_0ed859_p2_pairing_approve_target(const json_t *req) {
    const json_t *rid = json_obj_get(req, "request_id");
    const json_t *code = json_obj_get(req, "code");
    char platform[OPS_BIG], target[OPS_BIG], approve_arg[OPS_BIG];
    int by_request_id;
    json_t *out;

    /* platform = (body.platform or "").lower().strip() */
    snprintf(platform, sizeof(platform), "%s", ops_str_or_empty(json_obj_get(req, "platform")));
    ops_lower(platform);
    ops_strip(platform);

    /* target = (body.request_id or body.code or "").strip() */
    if (ops_truthy(rid))
        snprintf(target, sizeof(target), "%s", ops_str_or_empty(rid));
    else if (ops_truthy(code))
        snprintf(target, sizeof(target), "%s", ops_str_or_empty(code));
    else
        target[0] = '\0';
    ops_strip(target);

    if (!platform[0] || !target[0])
        return ops_verdict(0, 400, "platform and request_id or code are required");

    by_request_id = ops_truthy(rid) || ops_looks_like_request_id(target);
    snprintf(approve_arg, sizeof(approve_arg), "%s", target);
    if (!by_request_id) ops_upper(approve_arg);

    out = ops_verdict(1, 200, NULL);
    json_set(out, "platform", json_string(platform));
    json_set(out, "target", json_string(target));
    json_set(out, "by_request_id", json_bool(by_request_id ? true : false));
    json_set(out, "approve_arg", json_string(approve_arg));
    return out;
}

/* --- _dashboard_backup_path: the download authorization chain ------------ */

/* Both sides arrive already resolved, so containment is lexical: lowercase and
 * fold separators on nt, then compare component-wise (``_path_is_under`` is
 * ``target == root or root in target.parents``). */
static void ops_norm_path(const char *in, int nt, char *out, size_t cap) {
    size_t n = 0;
    const char *p;
    for (p = in ? in : ""; *p && n + 1 < cap; p++) {
        char c = *p;
        if (nt && c == '/') c = '\\';
        if (nt && c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        if (nt && c == '\\' && n > 0 && out[n - 1] == '\\') continue;
        out[n++] = c;
    }
    while (n > 1 && out[n - 1] == (nt ? '\\' : '/')) n--;
    out[n] = '\0';
}

static int ops_path_under(const char *root, const char *target, int nt) {
    char r[OPS_BIG], t[OPS_BIG];
    size_t rl;
    ops_norm_path(root, nt, r, sizeof(r));
    ops_norm_path(target, nt, t, sizeof(t));
    rl = strlen(r);
    if (rl == 0) return 0;
    if (strcmp(r, t) == 0) return 1;
    if (strncmp(r, t, rl) != 0) return 0;
    return t[rl] == (nt ? '\\' : '/');
}

json_t *w31_ops_0ed859_p2_backup_download_verdict(const json_t *req) {
    const json_t *resolved = json_obj_get(req, "resolved");
    const char *resolve_error = json_get_str(req, "resolve_error", "");
    const char *backup_dir = json_get_str(req, "backup_dir", "");
    int is_file = json_get_bool(req, "is_file", false) ? 1 : 0;
    int nt = json_get_bool(req, "nt", true) ? 1 : 0;

    /* Path(archive).resolve(strict=True) raises when the file is gone. */
    if (!strcmp(resolve_error, "not_found") || !resolved || resolved->type != JSON_STRING ||
        !resolved->str_val || !resolved->str_val[0])
        return ops_verdict(0, 404, "Backup not found");
    if (!strcmp(resolve_error, "invalid"))
        return ops_verdict(0, 400, "Invalid backup path");
    if (!ops_path_under(backup_dir, resolved->str_val, nt))
        return ops_verdict(0, 403, "Backup is outside the dashboard backup directory");
    if (!is_file) return ops_verdict(0, 404, "Backup not found");
    return ops_verdict(1, 200, NULL);
}
