/*
 * channel_directory.c — Cached map of reachable channels/contacts per platform.
 *
 * Port of Python gateway/channel_directory.py.
 *
 * Built on gateway startup, refreshed periodically (every 5 min), and saved to
 * ~/.hermes/channel_directory.json.  The send_message tool reads this file for
 * action="list" and for resolving human-friendly channel names to numeric IDs.
 *
 * Note: _build_discord, _build_slack, and build_channel_directory require
 * Python SDK access and are not directly portable to C. Channel discovery
 * from session history (_build_from_sessions) and the read/resolve functions
 * below provide the core functionality in C.
 */

#include "hermes.h"
#include "hermes_json.h"
#include "hermes_gateway.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>

#define DIRECTORY_FILENAME "channel_directory.json"

/* ================================================================
 *  Internal: get directory path
 * ================================================================ */

static const char *cd_path(void) {
    static char path[1024] = {0};
    if (path[0] == '\0') {
        const char *home = getenv("HERMES_HOME");
        if (!home) home = getenv("HOME");
        if (home) {
            snprintf(path, sizeof(path), "%s/%s", home, DIRECTORY_FILENAME);
        }
    }
    return path[0] ? path : NULL;
}

/* ================================================================
 *  Human-facing target label for a channel entry
 *  Port of Python _channel_target_name()
 *  AG26: Port of Python gateway/channel_directory.py:_channel_target_name().
 * ================================================================ */

char *channel_target_name(const char *platform_name, json_node_t *channel) {
    if (!channel || channel->type != JSON_OBJECT) return strdup("(unknown)");

    const char *name = json_object_get_string(channel, "name", "");
    if (!*name) return strdup("(unnamed)");

    char buf[512];
    const char *guild = json_object_get_string(channel, "guild", "");
    const char *type = json_object_get_string(channel, "type", "");

    if (platform_name && strcasecmp(platform_name, "discord") == 0 && *guild) {
        snprintf(buf, sizeof(buf), "#%s", name);
    } else if (platform_name && strcasecmp(platform_name, "discord") != 0 && *type) {
        snprintf(buf, sizeof(buf), "%s (%s)", name, type);
    } else {
        snprintf(buf, sizeof(buf), "%s", name);
    }

    return strdup(buf);
}

/* ================================================================
 *  Human-friendly session entry name
 *  Port of Python _session_entry_name()
 *  AG26: Port of Python gateway/channel_directory.py:_session_entry_name().
 * ================================================================ */

char *session_entry_name(json_node_t *origin) {
    if (!origin) return strdup("(unknown)");

    const char *base_name = json_object_get_string(origin, "chat_name", "");
    if (!*base_name) {
        base_name = json_object_get_string(origin, "user_name", "");
    }
    if (!*base_name) {
        base_name = json_object_get_string(origin, "chat_id", "(unknown)");
    }

    const char *thread_id = json_object_get_string(origin, "thread_id", "");
    if (!*thread_id) return strdup(base_name);

    const char *topic = json_object_get_string(origin, "chat_topic", "");
    char buf[512];
    if (*topic) {
        snprintf(buf, sizeof(buf), "%s / %s", base_name, topic);
    } else {
        snprintf(buf, sizeof(buf), "%s / topic %s", base_name, thread_id);
    }
    return strdup(buf);
}

/* ================================================================
 *  Build session-based channel entries from sessions.json
 *  Port of Python _build_from_sessions()
 *  AG26: Port of Python gateway/channel_directory.py:_build_from_sessions().
 *
 *  Returns a JSON array of channel entries for a given platform.
 *  Caller must json_free the result.
 * ================================================================ */

json_node_t *build_from_sessions(const char *platform_name) {
    json_node_t *channels = json_new_array();
    if (!channels) return NULL;

    const char *home = getenv("HERMES_HOME");
    if (!home) return channels;

    char sessions_path[1024];
    snprintf(sessions_path, sizeof(sessions_path),
             "%s/sessions/sessions.json", home);

    struct stat st;
    if (stat(sessions_path, &st) != 0) return channels;

    char *err = NULL;
    json_node_t *data = json_parse_file(sessions_path, &err);
    free(err);
    if (!data) return channels;

    json_t *jdata = (json_t *)data;
    size_t session_count = jdata->c.count;

    /* Simple address-based dedup set: we track seen entry IDs in a hash */
    /* Since we're dealing with session JSON objects (typically <100), linear scan is fine */
    char *seen_ids[256];
    int seen_count = 0;
    memset(seen_ids, 0, sizeof(seen_ids));

    for (size_t i = 0; i < session_count; i++) {
        json_node_t *session = jdata->c.items[i];
        if (!session || session->type != JSON_OBJECT) continue;

        json_node_t *origin = json_object_get(session, "origin");
        if (!origin || origin->type != JSON_OBJECT) continue;

        const char *plat = json_object_get_string(origin, "platform", "");
        if (strcasecmp(plat, platform_name) != 0) continue;

        char *entry_id = session_entry_id(
            json_object_get_string(origin, "chat_id", ""),
            json_object_get_string(origin, "thread_id", ""));
        if (!entry_id || !*entry_id) {
            free(entry_id);
            continue;
        }

        /* Dedup: skip if we've already seen this entry_id */
        bool already_seen = false;
        for (int si = 0; si < seen_count; si++) {
            if (strcmp(seen_ids[si], entry_id) == 0) {
                already_seen = true;
                break;
            }
        }
        if (already_seen) {
            free(entry_id);
            continue;
        }

        /* Track this entry ID */
        if (seen_count < 256) {
            seen_ids[seen_count++] = strdup(entry_id);
        }

        json_node_t *entry = json_new_object();
        json_object_set(entry, "id", json_new_string(entry_id));
        json_object_set(entry, "name", json_new_string(session_entry_name(origin)));
        json_object_set(entry, "type",
                        json_new_string(json_object_get_string(session, "chat_type", "dm")));
        const char *tid = json_object_get_string(origin, "thread_id", "");
        if (*tid) {
            json_object_set(entry, "thread_id", json_new_string(tid));
        }

        json_array_append(channels, entry);
        free(entry_id);
    }

    /* Cleanup dedup set */
    for (int si = 0; si < seen_count; si++) {
        free(seen_ids[si]);
    }

    json_free(data);
    return channels;
}

/* ================================================================
 *  Load the cached channel directory from disk
 *  Port of Python load_directory()
 *  AG26: Port of Python gateway/channel_directory.py:load_directory().
 *
 *  Returns a json_node_t: {"updated_at": "...", "platforms": {...}}
 *  Caller must json_free the result.
 * ================================================================ */

json_node_t *load_directory(void) {
    const char *path = cd_path();
    if (!path) {
        json_node_t *empty = json_new_object();
        json_object_set(empty, "updated_at", json_null());
        json_object_set(empty, "platforms", json_new_object());
        return empty;
    }

    struct stat st;
    if (stat(path, &st) != 0) {
        json_node_t *empty = json_new_object();
        json_object_set(empty, "updated_at", json_null());
        json_object_set(empty, "platforms", json_new_object());
        return empty;
    }

    char *err = NULL;
    json_node_t *root = json_parse_file(path, &err);
    free(err);
    if (!root) {
        json_node_t *empty = json_new_object();
        json_object_set(empty, "updated_at", json_null());
        json_object_set(empty, "platforms", json_new_object());
        return empty;
    }
    return root;
}

/* ================================================================
 *  Look up channel type for a chat_id
 *  Port of Python lookup_channel_type()
 *  AG26: Port of Python gateway/channel_directory.py:lookup_channel_type().
 *
 *  Returns malloc'd type string ("channel", "forum", "dm", etc.)
 *  or NULL if unknown. Caller must free.
 * ================================================================ */

char *lookup_channel_type(const char *platform_name, const char *chat_id) {
    if (!platform_name || !chat_id) return NULL;

    json_node_t *directory = load_directory();
    if (!directory) return NULL;

    json_node_t *platforms = json_object_get(directory, "platforms");
    if (!platforms || platforms->type != JSON_OBJECT) {
        json_free(directory);
        return NULL;
    }

    json_node_t *channels = json_object_get(platforms, platform_name);
    if (!channels || channels->type != JSON_ARRAY) {
        json_free(directory);
        return NULL;
    }

    json_t *arr = (json_t *)channels;
    char *result = NULL;
    for (size_t i = 0; i < arr->c.count; i++) {
        json_node_t *ch = arr->c.items[i];
        if (!ch || ch->type != JSON_OBJECT) continue;
        const char *cid = json_object_get_string(ch, "id", "");
        if (strcmp(cid, chat_id) == 0) {
            const char *type = json_object_get_string(ch, "type", "");
            if (*type) result = strdup(type);
            break;
        }
    }

    json_free(directory);
    return result;
}

/* ================================================================
 *  Resolve a human-friendly channel name to a numeric ID
 *  Port of Python resolve_channel_name()
 *  AG26: Port of Python gateway/channel_directory.py:resolve_channel_name().
 *
 *  Matching strategy (case-insensitive, first match wins):
 *  - Discord: "bot-home", "#bot-home", "GuildName/bot-home"
 *  - Telegram: display name or group name
 *  - Slack: "engineering", "#engineering"
 *
 *  Returns malloc'd ID string or NULL. Caller must free.
 * ================================================================ */

char *resolve_channel_name(const char *platform_name, const char *name) {
    if (!platform_name || !name || !*name) return NULL;

    json_node_t *directory = load_directory();
    if (!directory) return NULL;

    json_node_t *platforms = json_object_get(directory, "platforms");
    if (!platforms || platforms->type != JSON_OBJECT) {
        json_free(directory);
        return NULL;
    }

    json_node_t *channels = json_object_get(platforms, platform_name);
    if (!channels || channels->type != JSON_ARRAY) {
        json_free(directory);
        return NULL;
    }

    char *query = normalize_channel_query(name);
    if (!query) {
        json_free(directory);
        return NULL;
    }

    json_t *arr = (json_t *)channels;
    char *result = NULL;

    /* 0. Exact ID match */
    for (size_t i = 0; i < arr->c.count; i++) {
        json_node_t *ch = arr->c.items[i];
        if (!ch || ch->type != JSON_OBJECT) continue;
        const char *cid = json_object_get_string(ch, "id", "");
        if (strcmp(cid, name) == 0) {
            result = strdup(cid);
            goto done;
        }
    }

    /* 1. Exact name match */
    for (size_t i = 0; i < arr->c.count; i++) {
        json_node_t *ch = arr->c.items[i];
        if (!ch || ch->type != JSON_OBJECT) continue;
        const char *chname = json_object_get_string(ch, "name", "");
        char *norm = normalize_channel_query(chname);
        if (norm && strcmp(norm, query) == 0) {
            result = strdup(json_object_get_string(ch, "id", ""));
            free(norm);
            goto done;
        }
        free(norm);

        /* Also check display name */
        char *display = channel_target_name(platform_name, ch);
        char *norm_display = normalize_channel_query(display);
        if (norm_display && strcmp(norm_display, query) == 0) {
            result = strdup(json_object_get_string(ch, "id", ""));
            free(display);
            free(norm_display);
            goto done;
        }
        free(display);
        free(norm_display);
    }

    /* 2. Guild-qualified match for Discord ("GuildName/channel") */
    if (strchr(query, '/')) {
        char *slash = strchr(query, '/');
        if (slash) {
            *slash = '\0';
            const char *guild_part = query;
            const char *ch_part = slash + 1;
            for (size_t i = 0; i < arr->c.count; i++) {
                json_node_t *ch = arr->c.items[i];
                if (!ch || ch->type != JSON_OBJECT) continue;
                const char *guild = json_object_get_string(ch, "guild", "");
                char *norm_guild = normalize_channel_query(guild);
                if (norm_guild && strcmp(norm_guild, guild_part) == 0) {
                    const char *chname = json_object_get_string(ch, "name", "");
                    char *norm_chname = normalize_channel_query(chname);
                    if (norm_chname && strcmp(norm_chname, ch_part) == 0) {
                        result = strdup(json_object_get_string(ch, "id", ""));
                        free(norm_guild);
                        free(norm_chname);
                        goto done;
                    }
                    free(norm_chname);
                }
                free(norm_guild);
            }
        }
    }

    /* 3. Partial prefix match (only if unambiguous) */
    {
        char *match_id = NULL;
        int match_count = 0;
        size_t qlen = strlen(query);
        for (size_t i = 0; i < arr->c.count; i++) {
            json_node_t *ch = arr->c.items[i];
            if (!ch || ch->type != JSON_OBJECT) continue;
            const char *chname = json_object_get_string(ch, "name", "");
            char *norm = normalize_channel_query(chname);
            if (norm && strlen(norm) >= qlen && strncmp(norm, query, qlen) == 0) {
                match_count++;
                if (match_count == 1) {
                    match_id = strdup(json_object_get_string(ch, "id", ""));
                } else {
                    free(match_id);
                    match_id = NULL;
                }
            }
            free(norm);
        }
        if (match_count == 1) {
            result = match_id;
            goto done;
        }
        free(match_id);
    }

done:
    free(query);
    json_free(directory);
    return result;
}

/* ================================================================
 *  Format the channel directory as a human-readable list
 *  Port of Python format_directory_for_display()
 *  AG26: Port of Python gateway/channel_directory.py:format_directory_for_display().
 *
 *  Returns malloc'd string (caller must free).
 * ================================================================ */

char *format_directory_for_display(void) {
    json_node_t *directory = load_directory();
    if (!directory) return strdup("No channel directory available.");

    json_node_t *platforms = json_object_get(directory, "platforms");
    if (!platforms || platforms->type != JSON_OBJECT) {
        json_free(directory);
        return strdup("No messaging platforms connected or no channels discovered yet.");
    }

    char buf[8192] = {0};
    size_t pos = 0;

    json_t *pobj = (json_t *)platforms;
    bool any_data = false;

    for (size_t pi = 0; pi < pobj->c.count; pi++) {
        const char *plat_name = pobj->c.keys[pi];
        json_node_t *channels = pobj->c.items[pi];
        if (!channels || channels->type != JSON_ARRAY) continue;
        if (json_array_count(channels) == 0) continue;

        any_data = true;

        /* Discord: group by guild */
        if (strcasecmp(plat_name, "discord") == 0) {
            /* First pass: collect guilds and DMs */
            json_node_t *guilds_obj = json_new_object();
            json_node_t *dms_arr = json_new_array();
            json_t *charr = (json_t *)channels;
            for (size_t ci = 0; ci < charr->c.count; ci++) {
                json_node_t *ch = charr->c.items[ci];
                if (!ch || ch->type != JSON_OBJECT) continue;
                const char *guild = json_object_get_string(ch, "guild", "");
                if (*guild) {
                    json_node_t *glist = json_object_get(guilds_obj, guild);
                    if (!glist) {
                        glist = json_new_array();
                        json_object_set(guilds_obj, guild, glist);
                    }
                    json_array_append(glist, json_copy(ch));
                } else {
                    json_array_append(dms_arr, json_copy(ch));
                }
            }

            /* Render guilds */
            json_t *gobj = (json_t *)guilds_obj;
            for (size_t gi = 0; gi < gobj->c.count; gi++) {
                const char *gname = gobj->c.keys[gi];
                json_node_t *gchannels = gobj->c.items[gi];
                int n = snprintf(buf + pos, sizeof(buf) - pos, "Discord (%s):\n", gname);
                if (n > 0) pos += (size_t)n;

                json_t *garr = (json_t *)gchannels;
                for (size_t ci = 0; ci < garr->c.count; ci++) {
                    char *target = channel_target_name("discord", garr->c.items[ci]);
                    n = snprintf(buf + pos, sizeof(buf) - pos, "  discord:%s\n", target ? target : "");
                    free(target);
                    if (n > 0) pos += (size_t)n;
                }
            }

            /* Render DMs */
            json_t *darr = (json_t *)dms_arr;
            if (darr->c.count > 0) {
                int n = snprintf(buf + pos, sizeof(buf) - pos, "Discord (DMs):\n");
                if (n > 0) pos += (size_t)n;
                for (size_t ci = 0; ci < darr->c.count; ci++) {
                    char *target = channel_target_name("discord", darr->c.items[ci]);
                    n = snprintf(buf + pos, sizeof(buf) - pos, "  discord:%s\n", target ? target : "");
                    free(target);
                    if (n > 0) pos += (size_t)n;
                }
            }

            json_free(guilds_obj);
            json_free(dms_arr);
        } else {
            int n = snprintf(buf + pos, sizeof(buf) - pos, "%s:\n", plat_name);
            /* Title-case the first letter */
            if (n > 0 && buf[pos] >= 'a' && buf[pos] <= 'z')
                buf[pos] = (char)toupper((unsigned char)buf[pos]);
            if (n > 0) pos += (size_t)n;

            json_t *charr = (json_t *)channels;
            for (size_t ci = 0; ci < charr->c.count; ci++) {
                char *target = channel_target_name(plat_name, charr->c.items[ci]);
                if (target) {
                    n = snprintf(buf + pos, sizeof(buf) - pos, "  %s:%s\n", plat_name, target);
                    free(target);
                    if (n > 0) pos += (size_t)n;
                }
            }
        }

        int n = snprintf(buf + pos, sizeof(buf) - pos, "\n");
        if (n > 0) pos += (size_t)n;
    }

    json_free(directory);

    if (!any_data) {
        return strdup("No messaging platforms connected or no channels discovered yet.");
    }

    /* Append usage note */
    snprintf(buf + pos, sizeof(buf) - pos,
             "Use these as the \"target\" parameter when sending.\n"
             "Bare platform name (e.g. \"telegram\") sends to home channel.");

    return strdup(buf);
}
