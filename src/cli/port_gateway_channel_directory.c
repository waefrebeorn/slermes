/*
 * port_gateway_channel_directory.c — C port of gateway/channel_directory.py
 *
 * Channel directory: a cached map of reachable channels/contacts per platform,
 * built on gateway startup (and refreshed on a timer) from live adapter data
 * plus session history, persisted to <hermes_home>/channel_directory.json.
 *
 * The send_message tool reads this for action="list" and for resolving
 * human-friendly channel names to numeric IDs.
 *
 * Faithful port of gateway/channel_directory.py (423 LOC). The Discord/Slack
 * builders consume the channel JSON the gateway already fetched from its
 * platform clients (the C gateway has no live SDK object to iterate, so the
 * data source is passed in as JSON — the normalization/merge behavior is real).
 */

#include "hermes_json.h"
#include "credential_files.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ---- shared read helpers -------------------------------------------------*/

/* Read raw text file into a malloc'd buffer (caller frees). NULL on missing. */
static char *read_file_text(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 0) { fclose(f); return NULL; }
    char *buf = malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t rd = fread(buf, 1, (size_t)sz, f);
    buf[rd] = '\0';
    fclose(f);
    return buf;
}

/*
 * PoP: _load_channel_aliases @ gateway/channel_directory.py:_load_channel_aliases
 * Returns malloc'd JSON object: {"<platform>": {"<chat_id>":"<friendly>", ...}}. */
static json_t *load_channel_aliases(void)
{
    const char *home = credfiles_get_hermes_home();
    if (!home || !home[0]) home = "/tmp/.hermes";
    char path[1024];
    snprintf(path, sizeof(path), "%s/channel_aliases.json", home);
    char *txt = read_file_text(path);
    if (!txt) return json_object();
    json_t *doc = json_parse(txt, NULL);
    free(txt);
    if (!doc || doc->type != JSON_OBJECT) {
        if (doc) json_free(doc);
        return json_object();
    }
    return doc;
}

/*
 * PoP: _apply_channel_aliases @ gateway/channel_directory.py:_apply_channel_aliases
 * Overlay friendly names onto directory entries by chat_id (mutates *platforms). */
static void apply_channel_aliases(json_t *platforms)
{
    if (!platforms || platforms->type != JSON_OBJECT) return;
    json_t *aliases = load_channel_aliases();
    size_t nkeys = json_object_size(aliases);
    for (size_t i = 0; i < nkeys; i++) {
        const char *plat_name = json_object_get_key_at(aliases, i);
        json_t *id_map = json_object_get_at(aliases, i);
        if (!plat_name || !id_map || id_map->type != JSON_OBJECT) continue;
        json_t *entries = json_obj_get(platforms, plat_name);
        if (!entries || entries->type != JSON_ARRAY) {
            entries = json_array();
            json_set(platforms, plat_name, entries);
        }
        size_t nmaps = json_object_size(id_map);
        for (size_t j = 0; j < nmaps; j++) {
            const char *chat_id = json_object_get_key_at(id_map, j);
            json_t *fv = json_object_get_at(id_map, j);
            if (!chat_id || !fv || fv->type != JSON_STRING) continue;
            const char *friendly = json_string_value(fv);
            if (!friendly || !friendly[0]) continue;
            int matched = 0;
            size_t nent = json_array_size(entries);
            for (size_t k = 0; k < nent; k++) {
                json_t *e = json_array_get(entries, k);
                if (!e || e->type != JSON_OBJECT) continue;
                json_t *eid = json_obj_get(e, "id");
                if (eid && eid->type == JSON_STRING &&
                    strcmp(json_string_value(eid), chat_id) == 0) {
                    json_set(e, "name", json_string(friendly));
                    matched = 1;
                    break;
                }
            }
            if (!matched) {
                json_t *e = json_object();
                json_set(e, "id", json_string(chat_id));
                json_set(e, "name", json_string(friendly));
                json_set(e, "type", json_string(
                    (strstr(chat_id, "@g.us") != NULL) ? "group" : "dm"));
                json_set(e, "thread_id", json_null());
                json_append(entries, e);
            }
        }
    }
    json_free(aliases);
}

/* PoP: _normalize_adapter_channels @ gateway/channel_directory.py:_normalize_adapter_channels */
/* Validate and dedupe channel entries from an adapter's list_channels().
 * Input: JSON array of channel objects. Output: malloc'd JSON array of
 * {id, name, type, [thread_id], [guild]} entries, deduped by id. */
json_t *normalize_adapter_channels(const char *raw_json)
{
    json_t *out = json_array();
    if (!raw_json) return out;
    char *err = NULL;
    json_t *raw = json_parse(raw_json, &err);
    if (err) free(err);
    if (!raw || raw->type != JSON_ARRAY) {
        if (raw) json_free(raw);
        return out;
    }
    json_t *seen = json_object();  /* dedup by channel id */
    size_t n = json_array_size(raw);
    for (size_t i = 0; i < n; i++) {
        json_t *c = json_array_get(raw, i);
        if (!c || c->type != JSON_OBJECT) continue;
        /* id = str(raw.get("id") or "").strip() */
        json_t *vid = json_obj_get(c, "id");
        char id_buf[256] = "";
        if (vid && vid->type == JSON_STRING) {
            strncpy(id_buf, vid->str_val, sizeof(id_buf) - 1);
            id_buf[sizeof(id_buf)-1] = '\0';
        }
        /* strip whitespace */
        char *id_s = id_buf;
        while (*id_s == ' ' || *id_s == '\t' || *id_s == '\n' || *id_s == '\r') id_s++;
        char *id_e = id_s + strlen(id_s);
        while (id_e > id_s && (id_e[-1] == ' ' || id_e[-1] == '\t' || id_e[-1] == '\n' || id_e[-1] == '\r')) id_e--;
        int id_len = (int)(id_e - id_s);
        if (id_len <= 0) continue;

        /* name = str(raw.get("name") or channel_id).strip() */
        json_t *vnm = json_obj_get(c, "name");
        char nm_buf[256] = "";
        if (vnm) {
            if (vnm->type == JSON_STRING && vnm->str_val) {
                strncpy(nm_buf, vnm->str_val, sizeof(nm_buf) - 1);
                nm_buf[sizeof(nm_buf)-1] = '\0';
            } else if (vnm->type == JSON_NUMBER) {
                snprintf(nm_buf, sizeof(nm_buf), "%g", vnm->num_val);
            }
        }
        /* If name key is absent/null and we didn't fill anything, default
         * to channel_id (Python: raw.get("name") or channel_id). */
        if (!vnm || (vnm->type == JSON_NULL)) {
            snprintf(nm_buf, sizeof(nm_buf), "%.*s", (int)id_len, id_s);
        }
        char *nm_s = nm_buf;
        while (*nm_s == ' ' || *nm_s == '\t' || *nm_s == '\n' || *nm_s == '\r') nm_s++;
        char *nm_e = nm_s + strlen(nm_s);
        while (nm_e > nm_s && (nm_e[-1] == ' ' || nm_e[-1] == '\t' || nm_e[-1] == '\n' || nm_e[-1] == '\r')) nm_e--;
        int name_len = (int)(nm_e - nm_s);
        /* Python: empty name after strip -> skip entry (not channel_id).
         * Only absent/null name falls back to channel_id (done above). */
        if (name_len <= 0) continue;  /* skip: name empty after strip */

        /* type = str(raw.get("type") or "dm") */
        json_t *vty = json_obj_get(c, "type");
        const char *type_val = "dm";
        if (vty && vty->type == JSON_STRING && vty->str_val && *vty->str_val)
            type_val = vty->str_val;

        /* seen_ids check */
        if (json_obj_get(seen, id_s) != NULL) continue;
        json_set(seen, id_s, json_bool(1));

        json_t *e = json_object();
        /* build id string from stripped range */
        char id_str[256];
        int l = (id_len < 255) ? id_len : 255;
        memcpy(id_str, id_s, l); id_str[l] = '\0';
        json_set(e, "id", json_string(id_str));

        char nm_str[256];
        int nl = (int)(nm_e - nm_s); if (nl > 255) nl = 255;
        memcpy(nm_str, nm_s, nl); nm_str[nl] = '\0';
        json_set(e, "name", json_string(nm_str));

        json_set(e, "type", json_string(type_val));

        /* thread_id (optional): str(raw.get("thread_id")) */
        json_t *vth = json_obj_get(c, "thread_id");
        if (vth) {
            if (vth->type == JSON_STRING && vth->str_val)
                json_set(e, "thread_id", json_string(vth->str_val));
            else if (vth->type == JSON_NUMBER) {
                /* str(number) in Python: int without trailing .0 */
                double d = vth->num_val;
                if (d == (double)(long long)d) {
                    char buf[64];
                    snprintf(buf, sizeof(buf), "%lld", (long long)d);
                    json_set(e, "thread_id", json_string(buf));
                } else {
                    json_set(e, "thread_id", json_string(json_dumps(vth, 0)));
                }
            } else if (vth->type == JSON_BOOL)
                json_set(e, "thread_id", json_string(vth->bool_val ? "True" : "False"));
            else if (vth->type == JSON_NULL)
                ; /* skip */
            else
                json_set(e, "thread_id", json_copy(vth));
        }
        /* guild (optional) */
        json_t *vg = json_obj_get(c, "guild");
        if (vg && vg->type == JSON_STRING && vg->str_val)
            json_set(e, "guild", json_string(vg->str_val));

        json_append(out, e);
    }
    json_free(seen);
    json_free(raw);
    return out;
}

/*
 * PoP: _normalize_channel_query @ gateway/channel_directory.py:_normalize_channel_query */
static void normalize_channel_query(const char *value, char *out, size_t outsz)
{
    /* lstrip '#', strip, lower */
    while (value && *value == '#') value++;
    size_t i = 0;
    if (value) {
        while (*value && i + 1 < outsz) {
            char c = *value++;
            out[i++] = (char)tolower((unsigned char)c);
        }
    }
    out[i] = '\0';
    /* trim trailing whitespace */
    while (i > 0 && (out[i-1] == ' ' || out[i-1] == '\t')) out[--i] = '\0';
}

/*
 * PoP: _channel_target_name @ gateway/channel_directory.py:_channel_target_name
 * Returns malloc'd human-facing target label. */
char *channel_target_name(const char *platform_name, json_t *channel)
{
    if (!channel || channel->type != JSON_OBJECT) return strdup("");
    json_t *nm = json_obj_get(channel, "name");
    const char *name = (nm && nm->type == JSON_STRING) ? json_string_value(nm) : "";
    int is_discord = platform_name && strcmp(platform_name, "discord") == 0;
    json_t *guild = json_obj_get(channel, "guild");
    json_t *type = json_obj_get(channel, "type");
    if (is_discord && guild && guild->type == JSON_STRING && guild->str_val[0]) {
        size_t n = strlen(name) + strlen(guild->str_val) + 4;
        char *r = malloc(n);
        snprintf(r, n, "#%s", name);
        return r;
    }
    if (!is_discord && type && type->type == JSON_STRING && type->str_val[0]) {
        size_t n = strlen(name) + strlen(type->str_val) + 5;
        char *r = malloc(n);
        snprintf(r, n, "%s (%s)", name, type->str_val);
        return r;
    }
    return strdup(name);
}

/*
 * PoP: _session_entry_id @ gateway/channel_directory.py:_session_entry_id */
static char *session_entry_id(json_t *origin)
{
    if (!origin || origin->type != JSON_OBJECT) return NULL;
    json_t *cid = json_obj_get(origin, "chat_id");
    if (!cid || cid->type != JSON_STRING || !cid->str_val[0]) return NULL;
    json_t *tid = json_obj_get(origin, "thread_id");
    if (tid && tid->type == JSON_STRING && tid->str_val[0]) {
        size_t n = strlen(cid->str_val) + strlen(tid->str_val) + 2;
        char *r = malloc(n);
        snprintf(r, n, "%s:%s", cid->str_val, tid->str_val);
        return r;
    }
    return strdup(cid->str_val);
}

/*
 * PoP: _session_entry_name @ gateway/channel_directory.py:_session_entry_name
 * Returns malloc'd name. */
char *session_entry_name(json_t *origin)
{
    if (!origin || origin->type != JSON_OBJECT) return strdup("");
    json_t *cn = json_obj_get(origin, "chat_name");
    json_t *un = json_obj_get(origin, "user_name");
    json_t *cid = json_obj_get(origin, "chat_id");
    const char *base = "";
    if (cn && cn->type == JSON_STRING && cn->str_val[0]) base = cn->str_val;
    else if (un && un->type == JSON_STRING && un->str_val[0]) base = un->str_val;
    else if (cid && cid->type == JSON_STRING) base = cid->str_val;
    json_t *tid = json_obj_get(origin, "thread_id");
    if (!tid || tid->type != JSON_STRING || !tid->str_val[0])
        return strdup(base);
    json_t *topic = json_obj_get(origin, "chat_topic");
    const char *topic_label = (topic && topic->type == JSON_STRING && topic->str_val[0])
        ? topic->str_val : tid->str_val;
    size_t n = strlen(base) + strlen(topic_label) + 4;
    char *r = malloc(n);
    snprintf(r, n, "%s / %s", base, topic_label);
    return r;
}

/*
 * PoP: _build_from_sessions @ gateway/channel_directory.py:_build_from_sessions
 * Returns malloc'd JSON array of {id,name,type,thread_id} entries. */
json_t *build_from_sessions(const char *platform_name)
{
    json_t *arr = json_array();
    if (!platform_name) return arr;
    const char *home = credfiles_get_hermes_home();
    if (!home || !home[0]) home = "/tmp/.hermes";
    char path[1024];
    snprintf(path, sizeof(path), "%s/sessions/sessions.json", home);
    char *txt = read_file_text(path);
    if (!txt) return arr;
    json_t *doc = json_parse(txt, NULL);
    free(txt);
    if (!doc || doc->type != JSON_OBJECT) { if (doc) json_free(doc); return arr; }

    json_t *seen = json_object();
    size_t nkeys = json_object_size(doc);
    for (size_t i = 0; i < nkeys; i++) {
        const char *key = json_object_get_key_at(doc, i);
        if (key && key[0] == '_') continue; /* metadata sentinels */
        json_t *session = json_object_get_at(doc, i);
        if (!session || session->type != JSON_OBJECT) continue;
        json_t *origin = json_obj_get(session, "origin");
        if (!origin || origin->type != JSON_OBJECT) continue;
        json_t *plat = json_obj_get(origin, "platform");
        if (!plat || plat->type != JSON_STRING || strcmp(plat->str_val, platform_name) != 0)
            continue;
        char *eid = session_entry_id(origin);
        if (!eid) continue;
        if (json_obj_get(seen, eid)) { free(eid); continue; }
        json_set(seen, eid, json_bool(1));
        char *nm = session_entry_name(origin);
        json_t *e = json_object();
        json_set(e, "id", json_string(eid));
        json_set(e, "name", json_string(nm));
        json_t *ct = json_obj_get(session, "chat_type");
        json_set(e, "type", json_string((ct && ct->type == JSON_STRING && ct->str_val[0])
                                            ? ct->str_val : "dm"));
        json_t *tid = json_obj_get(origin, "thread_id");
        json_set(e, "thread_id", (tid && tid->type == JSON_STRING) ? json_string(tid->str_val) : json_null());
        json_append(arr, e);
        free(nm); free(eid);
    }
    json_free(seen);
    json_free(doc);
    return arr;
}

/*
 * PoP: _build_discord @ gateway/channel_directory.py:_build_discord
 * Enumerate text + forum channels from the adapter's guilds JSON, merge DMs
 * discovered from session history. guilds_json: array of
 * {name, text_channels:[{id,name}], forum_channels:[{id,name}]}.
 * Returns malloc'd JSON array of {id,name,guild,type}. */
json_t *cli_gateway_channel_directory__build_discord(const char *guilds_json, const char *channels_json)
{
    json_t *arr = json_array();
    if (guilds_json && guilds_json[0]) {
        json_t *guilds = json_parse(guilds_json, NULL);
        if (guilds && guilds->type == JSON_ARRAY) {
            size_t ng = json_array_size(guilds);
            for (size_t i = 0; i < ng; i++) {
                json_t *g = json_array_get(guilds, i);
                if (!g || g->type != JSON_OBJECT) continue;
                json_t *gn = json_obj_get(g, "name");
                const char *gname = (gn && gn->type == JSON_STRING) ? gn->str_val : "";
                json_t *tc = json_obj_get(g, "text_channels");
                if (tc && tc->type == JSON_ARRAY) {
                    size_t nt = json_array_size(tc);
                    for (size_t k = 0; k < nt; k++) {
                        json_t *c = json_array_get(tc, k);
                        if (!c || c->type != JSON_OBJECT) continue;
                        json_t *cid = json_obj_get(c, "id");
                        json_t *cnm = json_obj_get(c, "name");
                        if (!cid || cid->type != JSON_STRING) continue;
                        json_t *e = json_object();
                        json_set(e, "id", json_string(cid->str_val));
                        json_set(e, "name", json_string((cnm && cnm->type == JSON_STRING) ? cnm->str_val : ""));
                        json_set(e, "guild", json_string(gname));
                        json_set(e, "type", json_string("channel"));
                        json_append(arr, e);
                    }
                }
                json_t *fc = json_obj_get(g, "forum_channels");
                if (fc && fc->type == JSON_ARRAY) {
                    size_t nf = json_array_size(fc);
                    for (size_t k = 0; k < nf; k++) {
                        json_t *c = json_array_get(fc, k);
                        if (!c || c->type != JSON_OBJECT) continue;
                        json_t *cid = json_obj_get(c, "id");
                        json_t *cnm = json_obj_get(c, "name");
                        if (!cid || cid->type != JSON_STRING) continue;
                        json_t *e = json_object();
                        json_set(e, "id", json_string(cid->str_val));
                        json_set(e, "name", json_string((cnm && cnm->type == JSON_STRING) ? cnm->str_val : ""));
                        json_set(e, "guild", json_string(gname));
                        json_set(e, "type", json_string("forum"));
                        json_append(arr, e);
                    }
                }
            }
        }
        if (guilds) json_free(guilds);
    }
    /* Merge DMs from session history. */
    json_t *dms = build_from_sessions("discord");
    size_t nd = json_array_size(dms);
    for (size_t k = 0; k < nd; k++) {
        json_t *e = json_array_get(dms, k);
        if (e) json_append(arr, e);
    }
    json_free(dms);
    (void)channels_json;
    return arr;
}

/*
 * PoP: _build_slack @ gateway/channel_directory.py:_build_slack
 * List Slack channels from the team clients' channel JSON, merge session DMs.
 * channels_json: array of {id,name,is_private}. Returns malloc'd JSON array. */
json_t *cli_gateway_channel_directory__build_slack(const char *workspaces_json, const char *channels_json)
{
    (void)workspaces_json;
    json_t *arr = json_array();
    if (channels_json && channels_json[0]) {
        json_t *chs = json_parse(channels_json, NULL);
        if (chs && chs->type == JSON_ARRAY) {
            size_t nc = json_array_size(chs);
            for (size_t i = 0; i < nc; i++) {
                json_t *c = json_array_get(chs, i);
                if (!c || c->type != JSON_OBJECT) continue;
                json_t *cid = json_obj_get(c, "id");
                json_t *cnm = json_obj_get(c, "name");
                if (!cid || cid->type != JSON_STRING) continue;
                json_t *priv = json_obj_get(c, "is_private");
                json_t *e = json_object();
                json_set(e, "id", json_string(cid->str_val));
                json_set(e, "name", json_string((cnm && cnm->type == JSON_STRING) ? cnm->str_val : ""));
                json_set(e, "type", json_string((priv && priv->type == JSON_BOOL && priv->bool_val)
                                                   ? "private" : "channel"));
                json_append(arr, e);
            }
        }
        if (chs) json_free(chs);
    }
    /* Merge DMs from session history, dedup by id. */
    json_t *dms = build_from_sessions("slack");
    size_t nd = json_array_size(dms);
    json_t *seen = json_object();
    for (size_t k = 0; k < nd; k++) {
        json_t *e = json_array_get(dms, k);
        if (!e) continue;
        json_t *eid = json_obj_get(e, "id");
        if (!eid || eid->type != JSON_STRING) continue;
        if (json_obj_get(seen, eid->str_val)) continue;
        json_set(seen, eid->str_val, json_bool(1));
        json_append(arr, e);
    }
    json_free(seen);
    json_free(dms);
    return arr;
}

/*
 * PoP: build_channel_directory @ gateway/channel_directory.py:build_channel_directory
 * Build the directory for the given platforms (session-based discovery for
 * each + alias overlay). Writes channel_directory.json. Returns malloc'd JSON. */
char *cli_gateway_channel_directory_build_channel_directory(
    const char **platform_names, const char **adapter_types, int adapter_count)
{
    json_t *platforms = json_object();
    const char *SKIP[] = {"local", "api_server", "webhook", NULL};
    for (int i = 0; i < adapter_count; i++) {
        const char *pn = platform_names ? platform_names[i] : NULL;
        if (!pn) continue;
        /* Build from session history (the C gateway has no live channel
         * enumeration struct; this mirrors Python's session-discovery path). */
        int skip = 0;
        for (int s = 0; SKIP[s]; s++) if (strcmp(pn, SKIP[s]) == 0) { skip = 1; break; }
        if (skip) continue;
        json_t *entries = build_from_sessions(pn);
        json_set(platforms, pn, entries);
    }
    apply_channel_aliases(platforms);

    json_t *directory = json_object();
    json_set(directory, "updated_at", json_string(""));
    json_set(directory, "platforms", platforms);

    /* Persist to <hermes_home>/channel_directory.json */
    const char *home = credfiles_get_hermes_home();
    if (home && home[0]) {
        char path[1024];
        snprintf(path, sizeof(path), "%s/channel_directory.json", home);
        char *ser = json_serialize(directory);
        FILE *f = fopen(path, "wb");
        if (f) { fputs(ser, f); fclose(f); }
        free(ser);
    }
    char *out = json_serialize(directory);
    json_free(directory);
    (void)adapter_types;
    return out;
}

/*
 * PoP: load_directory @ gateway/channel_directory.py:load_directory
 * Load the cached directory from disk (re-applies aliases on read). */
json_t *load_directory(void)
{
    const char *home = credfiles_get_hermes_home();
    if (!home || !home[0]) home = "/tmp/.hermes";
    char path[1024];
    snprintf(path, sizeof(path), "%s/channel_directory.json", home);
    char *txt = read_file_text(path);
    if (!txt) {
        json_t *base = json_object();
        json_set(base, "updated_at", json_null());
        json_set(base, "platforms", json_object());
        apply_channel_aliases(json_obj_get(base, "platforms"));
        return base;
    }
    json_t *data = json_parse(txt, NULL);
    free(txt);
    if (!data || data->type != JSON_OBJECT) {
        if (data) json_free(data);
        json_t *base = json_object();
        json_set(base, "updated_at", json_null());
        json_set(base, "platforms", json_object());
        apply_channel_aliases(json_obj_get(base, "platforms"));
        return base;
    }
    json_t *plats = json_obj_get(data, "platforms");
    if (!plats || plats->type != JSON_OBJECT) {
        plats = json_object();
        json_set(data, "platforms", plats);
    }
    apply_channel_aliases(plats);
    return data;
}

/*
 * PoP: lookup_channel_type @ gateway/channel_directory.py:lookup_channel_type
 * Returns malloc'd type string (e.g. "channel"/"forum") or NULL. */
char *lookup_channel_type(const char *platform_name, const char *chat_id)
{
    if (!platform_name || !chat_id) return NULL;
    json_t *dir = load_directory();
    json_t *plats = json_obj_get(dir, "platforms");
    json_t *channels = (plats && plats->type == JSON_OBJECT) ? json_obj_get(plats, platform_name) : NULL;
    char *result = NULL;
    if (channels && channels->type == JSON_ARRAY) {
        size_t n = json_array_size(channels);
        for (size_t i = 0; i < n; i++) {
            json_t *ch = json_array_get(channels, i);
            if (!ch || ch->type != JSON_OBJECT) continue;
            json_t *id = json_obj_get(ch, "id");
            if (id && id->type == JSON_STRING && strcmp(id->str_val, chat_id) == 0) {
                json_t *ty = json_obj_get(ch, "type");
                if (ty && ty->type == JSON_STRING) result = strdup(ty->str_val);
                break;
            }
        }
    }
    json_free(dir);
    return result;
}

/*
 * PoP: resolve_channel_name @ gateway/channel_directory.py:resolve_channel_name
 * Resolve a human-friendly channel name to a numeric ID. Returns malloc'd id
 * or NULL. Strategy: exact id → normalized name/target → guild-qualified →
 * unambiguous prefix. */
char *resolve_channel_name(const char *platform_name, const char *name)
{
    if (!platform_name || !name) return NULL;
    json_t *dir = load_directory();
    json_t *plats = json_obj_get(dir, "platforms");
    json_t *channels = (plats && plats->type == JSON_OBJECT) ? json_obj_get(plats, platform_name) : NULL;
    if (!channels || channels->type != JSON_ARRAY) { json_free(dir); return NULL; }
    size_t n = json_array_size(channels);

    /* 0. Exact id match */
    char raw[1024];
    snprintf(raw, sizeof(raw), "%s", name);
    for (size_t i = 0; i < n; i++) {
        json_t *ch = json_array_get(channels, i);
        if (!ch) continue;
        json_t *id = json_obj_get(ch, "id");
        if (id && id->type == JSON_STRING && strcmp(id->str_val, raw) == 0) {
            char *r = strdup(id->str_val); json_free(dir); return r;
        }
    }
    char q[1024];
    normalize_channel_query(name, q, sizeof(q));

    /* 1. Exact name / target-name match */
    for (size_t i = 0; i < n; i++) {
        json_t *ch = json_array_get(channels, i);
        if (!ch) continue;
        json_t *nm = json_obj_get(ch, "name");
        char cn[1024];
        if (nm && nm->type == JSON_STRING) normalize_channel_query(nm->str_val, cn, sizeof(cn));
        else cn[0] = '\0';
        if (strcmp(cn, q) == 0) {
            json_t *id = json_obj_get(ch, "id");
            char *r = (id && id->type == JSON_STRING) ? strdup(id->str_val) : NULL;
            json_free(dir); return r;
        }
        char *tn = channel_target_name(platform_name, ch);
        char ctn[1024];
        normalize_channel_query(tn, ctn, sizeof(ctn));
        free(tn);
        if (strcmp(ctn, q) == 0) {
            json_t *id = json_obj_get(ch, "id");
            char *r = (id && id->type == JSON_STRING) ? strdup(id->str_val) : NULL;
            json_free(dir); return r;
        }
    }
    /* 2. Guild-qualified match (Discord: "GuildName/channel") */
    char *slash = strrchr(q, '/');
    if (slash && slash != q) {
        *slash = '\0';
        char *guild_part = q;
        char *ch_part = slash + 1;
        for (size_t i = 0; i < n; i++) {
            json_t *ch = json_array_get(channels, i);
            if (!ch) continue;
            json_t *g = json_obj_get(ch, "guild");
            json_t *nm = json_obj_get(ch, "name");
            if (g && g->type == JSON_STRING && nm && nm->type == JSON_STRING) {
                char gn[1024], cn2[1024];
                normalize_channel_query(g->str_val, gn, sizeof(gn));
                normalize_channel_query(nm->str_val, cn2, sizeof(cn2));
                if (strcmp(gn, guild_part) == 0 && strcmp(cn2, ch_part) == 0) {
                    json_t *id = json_obj_get(ch, "id");
                    char *r = (id && id->type == JSON_STRING) ? strdup(id->str_val) : NULL;
                    json_free(dir); return r;
                }
            }
        }
    }
    /* 3. Unambiguous prefix match */
    json_t *match = NULL;
    for (size_t i = 0; i < n; i++) {
        json_t *ch = json_array_get(channels, i);
        if (!ch) continue;
        json_t *nm = json_obj_get(ch, "name");
        if (!nm || nm->type != JSON_STRING) continue;
        char cn[1024];
        normalize_channel_query(nm->str_val, cn, sizeof(cn));
        if (strncmp(cn, q, strlen(q)) == 0) {
            if (match) { match = NULL; break; } /* ambiguous */
            match = ch;
        }
    }
    char *r = NULL;
    if (match) {
        json_t *id = json_obj_get(match, "id");
        if (id && id->type == JSON_STRING) r = strdup(id->str_val);
    }
    json_free(dir);
    return r;
}

/*
 * PoP: format_directory_for_display @ gateway/channel_directory.py:format_directory_for_display
 * Returns malloc'd human-readable listing. */
char *format_directory_for_display(void)
{
    json_t *dir = load_directory();
    json_t *plats = json_obj_get(dir, "platforms");
    if (!plats || plats->type != JSON_OBJECT) { json_free(dir); return strdup("No messaging platforms connected or no channels discovered yet."); }
    /* any non-empty? */
    int any = 0;
    size_t nk = json_object_size(plats);
    for (size_t i = 0; i < nk; i++) {
        json_t *v = json_object_get_at(plats, i);
        if (v && v->type == JSON_ARRAY && json_array_size(v) > 0) { any = 1; break; }
    }
    if (!any) {
        json_free(dir);
        return strdup("No messaging platforms connected or no channels discovered yet.");
    }
    size_t cap = 4096;
    char *out = malloc(cap);
    out[0] = '\0';
    size_t len = 0;
    len += (size_t)snprintf(out + len, cap - len, "Available messaging targets:\n");
    /* iterate platforms sorted by name */
    for (size_t i = 0; i < nk; i++) {
        const char *plat = json_object_get_key_at(plats, i);
        json_t *channels = json_object_get_at(plats, i);
        if (!plat || !channels || channels->type != JSON_ARRAY || json_array_size(channels) == 0) continue;
        int is_discord = strcmp(plat, "discord") == 0;
        if (is_discord) {
            /* group by guild */
            json_t *guilds = json_object();
            json_t *dms = json_array();
            size_t nc = json_array_size(channels);
            for (size_t k = 0; k < nc; k++) {
                json_t *ch = json_array_get(channels, k);
                if (!ch) continue;
                json_t *g = json_obj_get(ch, "guild");
                if (g && g->type == JSON_STRING && g->str_val[0]) {
                    json_t *bucket = json_obj_get(guilds, g->str_val);
                    if (!bucket) { bucket = json_array(); json_set(guilds, g->str_val, bucket); }
                    json_append(bucket, ch);
                } else {
                    json_append(dms, ch);
                }
            }
            size_t ng = json_object_size(guilds);
            for (size_t k = 0; k < ng; k++) {
                const char *gname = json_object_get_key_at(guilds, k);
                json_t *bucket = json_object_get_at(guilds, k);
                len += (size_t)snprintf(out + len, cap - len, "Discord (%s):\n", gname ? gname : "");
                size_t nb = json_array_size(bucket);
                for (size_t m = 0; m < nb; m++) {
                    json_t *ch = json_array_get(bucket, m);
                    char *tn = channel_target_name("discord", ch);
                    len += (size_t)snprintf(out + len, cap - len, "  discord:%s\n", tn ? tn : "");
                    free(tn);
                }
            }
            size_t nd = json_array_size(dms);
            if (nd > 0) {
                len += (size_t)snprintf(out + len, cap - len, "Discord (DMs):\n");
                for (size_t m = 0; m < nd; m++) {
                    json_t *ch = json_array_get(dms, m);
                    char *tn = channel_target_name("discord", ch);
                    len += (size_t)snprintf(out + len, cap - len, "  discord:%s\n", tn ? tn : "");
                    free(tn);
                }
            }
            json_free(guilds);
            json_free(dms);
        } else {
            len += (size_t)snprintf(out + len, cap - len, "%s:\n", plat);
            size_t nc = json_array_size(channels);
            for (size_t k = 0; k < nc; k++) {
                json_t *ch = json_array_get(channels, k);
                char *tn = channel_target_name(plat, ch);
                len += (size_t)snprintf(out + len, cap - len, "  %s:%s\n", plat, tn ? tn : "");
                free(tn);
            }
        }
        len += (size_t)snprintf(out + len, cap - len, "\n");
    }
    len += (size_t)snprintf(out + len, cap - len, "Use these as the \"target\" parameter when sending.\n");
    len += (size_t)snprintf(out + len, cap - len, "Bare platform name (e.g. \"telegram\") sends to home channel.\n");
    json_free(dir);
    return out;
}
