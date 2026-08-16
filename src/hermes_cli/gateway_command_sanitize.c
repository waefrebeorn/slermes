/*
 * gateway_command_sanitize.c — pure gateway command-name sanitization,
 * length clamping, and Telegram menu prioritization.
 *
 * Faithful C11 port of the pure helpers in hermes_cli/commands.py. See
 * gateway_command_sanitize.h for the full contract. The registry/plugin/skill
 * collection parts of commands.py remain REAL_GAP; this module is intentionally
 * narrow and self-contained (no god header, no runtime deps beyond libjson).
 */

#include "gateway_command_sanitize.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ── Built-in Telegram menu priority (mirrors _TELEGRAM_MENU_PRIORITY) ── */

static const char *TELEGRAM_MENU_PRIORITY[] = {
    "help", "new", "stop", "status", "resume", "sessions", "model",
    "debug", "restart", "update", "verbose", "commands",
    "approve", "deny", "queue", "steer", "background",
    "reasoning", "usage", "platforms", "platform", "profile", "whoami",
    NULL
};

#define DEFAULT_TELEGRAM_MENU_MAX_COMMANDS 60
#define TELEGRAM_BOT_API_MAX_COMMANDS 100
#define TG_PRIORITY_MODE_PREPEND 0
#define TG_PRIORITY_MODE_APPEND 1
#define TG_PRIORITY_MODE_REPLACE 2

/* ── cmd_entry_t ────────────────────────────────────────────────────── */

cmd_entry_t *cmd_entry_make(const char *name, const char *desc, const char *key) {
    cmd_entry_t *e = calloc(1, sizeof(*e));
    if (!e) return NULL;
    size_t nlen = name ? strlen(name) : 0;
    if (nlen > CMD_NAME_LIMIT) nlen = CMD_NAME_LIMIT;
    if (nlen) memcpy(e->name, name, nlen);
    e->name[nlen] = '\0';
    if (desc) e->description = strdup(desc);
    if (key) e->key = strdup(key);
    return e;
}
/* Free a cmd_entry_t's owned heap members (description/key). The struct itself
 * is caller-owned (it may be stack-allocated or embedded), so it is NOT freed
 * here. For a heap entry created by cmd_entry_make(), call cmd_entry_free(e)
 * then free(e). */
void cmd_entry_free(cmd_entry_t *e) {
    if (!e) return;
    free(e->description);
    free(e->key);
    e->description = NULL;
    e->key = NULL;
}

/* ── Name sanitizers ───────────────────────────────────────────────── */

/* PoP: commands_sanitize_telegram_name @ hermes_cli/commands.py:_sanitize_telegram_name */
char *commands_sanitize_telegram_name(const char *raw) {
    if (!raw) return strdup("");
    size_t n = strlen(raw);
    char *buf = malloc(n + 1);
    if (!buf) return strdup("");
    size_t o = 0;
    for (size_t i = 0; i < n; i++) {
        char c = (char)tolower((unsigned char)raw[i]);
        if (c == '-') c = '_';
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')
            buf[o++] = c;
    }
    buf[o] = '\0';
    /* collapse consecutive underscores */
    size_t w = 0;
    bool prev_underscore = false;
    for (size_t i = 0; i < o; i++) {
        if (buf[i] == '_') {
            if (prev_underscore) continue;
            prev_underscore = true;
        } else {
            prev_underscore = false;
        }
        buf[w++] = buf[i];
    }
    buf[w] = '\0';
    /* strip leading/trailing underscores */
    size_t s = 0, e = w;
    while (s < e && buf[s] == '_') s++;
    while (e > s && buf[e - 1] == '_') e--;
    memmove(buf, buf + s, e - s);
    buf[e - s] = '\0';
    return buf;
}

/* PoP: commands_sanitize_slack_name @ hermes_cli/commands.py:_sanitize_slack_name */
char *commands_sanitize_slack_name(const char *raw) {
    if (!raw) return strdup("");
    size_t n = strlen(raw);
    char *buf = malloc(n + 1);
    if (!buf) return strdup("");
    size_t o = 0;
    for (size_t i = 0; i < n; i++) {
        char c = (char)tolower((unsigned char)raw[i]);
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_')
            buf[o++] = c;
    }
    buf[o] = '\0';
    /* trim leading/trailing '-'/'_' */
    size_t s = 0, e = o;
    while (s < e && (buf[s] == '-' || buf[s] == '_')) s++;
    while (e > s && (buf[e - 1] == '-' || buf[e - 1] == '_')) e--;
    memmove(buf, buf + s, e - s);
    buf[e - s] = '\0';
    /* cap at 32 chars */
    if (e - s > CMD_NAME_LIMIT) buf[CMD_NAME_LIMIT] = '\0';
    return buf;
}

/* PoP: commands_requires_argument @ hermes_cli/commands.py:_requires_argument */
bool commands_requires_argument(const char *args_hint) {
    if (!args_hint) return false;
    while (*args_hint == ' ' || *args_hint == '\t') args_hint++;
    return *args_hint == '<';
}

/* ── Human-readable file size label (mirrors _file_size_label) ── */

/* PoP: commands_file_size_label @ hermes_cli/commands.py:_file_size_label */
char *commands_file_size_label(long size) {
    char buf[32];
    if (size < 1024) {
        snprintf(buf, sizeof(buf), "%ldB", size);
    } else if (size < 1024L * 1024) {
        snprintf(buf, sizeof(buf), "%.0fK", (double)size / 1024.0);
    } else if (size < 1024L * 1024 * 1024) {
        snprintf(buf, sizeof(buf), "%.1fM", (double)size / (1024.0 * 1024.0));
    } else {
        snprintf(buf, sizeof(buf), "%.1fG", (double)size / (1024.0 * 1024.0 * 1024.0));
    }
    return strdup(buf);
}

/* ── Dedupe sanitized names ────────────────────────────────────────── */

char **commands_dedupe_sanitized_telegram(const char *const *names, int n, int *out_n) {
    char **out = NULL;
    int cnt = 0;
    if (out_n) *out_n = 0;
    if (!names || n <= 0) return NULL;
    out = malloc(sizeof(char *) * (size_t)n);
    if (!out) return NULL;
    for (int i = 0; i < n; i++) {
        char *s = commands_sanitize_telegram_name(names[i]);
        if (!s || !s[0]) { free(s); continue; }
        bool seen = false;
        for (int j = 0; j < cnt; j++) if (strcmp(out[j], s) == 0) { seen = true; break; }
        if (seen) { free(s); continue; }
        out[cnt++] = s;
    }
    if (out_n) *out_n = cnt;
    return out;
}

/* ── Telegram command-menu config ─────────────────────────────────── */

/* Parse the command-menu config. *menu_cfg_json* is the already-extracted
 * `command_menu` object (the JSON dict the Python originals return from
 * _telegram_command_menu_config). Passing the full nested config here is NOT
 * required — the caller drills into platforms.telegram.extra.command_menu and
 * hands us that subtree, which keeps this module free of the deep-nesting the
 * bundled libjson cannot parse. Fills out_max, out_mode (TG_PRIORITY_MODE_*),
 * and a malloc'd priority array (caller frees each string + the array). */
static void parse_menu_config(const char *menu_cfg_json, int *out_max,
                               int *out_mode, char ***out_priority, int *out_npri) {
    int max_commands = DEFAULT_TELEGRAM_MENU_MAX_COMMANDS;
    int mode = TG_PRIORITY_MODE_PREPEND;
    char **priority = NULL;
    int npri = 0;

    if (menu_cfg_json && *menu_cfg_json) {
        json_t *menu = json_parse(menu_cfg_json, NULL);
        if (menu && menu->type == JSON_OBJECT) {
            json_t *mc = json_obj_get(menu, "max_commands");
            if (mc && mc->type == JSON_NUMBER) {
                int v = (int)mc->num_val;
                if (v < 1) v = 1;
                if (v > TELEGRAM_BOT_API_MAX_COMMANDS) v = TELEGRAM_BOT_API_MAX_COMMANDS;
                max_commands = v;
            }
            json_t *pm = json_obj_get(menu, "priority_mode");
            if (pm && pm->type == JSON_STRING) {
                const char *s = pm->str_val;
                if (strcmp(s, "append") == 0) mode = TG_PRIORITY_MODE_APPEND;
                else if (strcmp(s, "replace") == 0) mode = TG_PRIORITY_MODE_REPLACE;
                else mode = TG_PRIORITY_MODE_PREPEND;
            }
            json_t *pr = json_obj_get(menu, "priority");
            if (pr && pr->type == JSON_ARRAY) {
                size_t cnt = pr->c.count;
                priority = malloc(sizeof(char *) * (cnt ? cnt : 1));
                for (size_t i = 0; i < cnt; i++) {
                    json_t *it = pr->c.items[i];
                    if (it && it->type == JSON_STRING && it->str_val && *it->str_val) {
                        priority[npri++] = strdup(it->str_val);
                    }
                }
            }
        }
        if (menu) json_free(menu);
    }
    if (!priority) priority = malloc(sizeof(char *));
    *out_max = max_commands;
    *out_mode = mode;
    *out_priority = priority;
    *out_npri = npri;
}

char *commands_telegram_menu_config_json(const char *raw_cfg_json) {
    int max_commands, mode, npri;
    char **priority;
    parse_menu_config(raw_cfg_json, &max_commands, &mode, &priority, &npri);

    json_t *obj = json_object();
    json_set(obj, "max_commands", json_number((double)max_commands));
    const char *md = mode == TG_PRIORITY_MODE_APPEND ? "append"
                   : mode == TG_PRIORITY_MODE_REPLACE ? "replace" : "prepend";
    json_set(obj, "priority_mode", json_string(md));
    json_t *parr = json_array();
    for (int i = 0; i < npri; i++) json_append(parr, json_string(priority[i]));
    json_set(obj, "priority", parr);

    char *out = json_serialize(obj);
    json_free(obj);
    for (int i = 0; i < npri; i++) free(priority[i]);
    free(priority);
    return out;
}

/* PoP: commands_telegram_menu_max_commands @ hermes_cli/commands.py:telegram_menu_max_commands */
int commands_telegram_menu_max_commands(const char *raw_cfg_json) {
    int max_commands, mode, npri;
    char **priority;
    parse_menu_config(raw_cfg_json, &max_commands, &mode, &priority, &npri);
    for (int i = 0; i < npri; i++) free(priority[i]);
    free(priority);
    return max_commands;
}

char **commands_telegram_effective_priority(const char *raw_cfg_json, int *out_n) {
    int max_commands, mode, npri;
    char **priority;
    parse_menu_config(raw_cfg_json, &max_commands, &mode, &priority, &npri);

    /* Build dedupe'd configured + default lists. */
    char **configured; int ncfg;
    configured = commands_dedupe_sanitized_telegram((const char *const *)priority, npri, &ncfg);
    char **defaults; int ndef;
    defaults = commands_dedupe_sanitized_telegram(TELEGRAM_MENU_PRIORITY,
                                                  (int)(sizeof(TELEGRAM_MENU_PRIORITY)/sizeof(*TELEGRAM_MENU_PRIORITY) - 1),
                                                  &ndef);

    int rc = 0;
    char **result = malloc(sizeof(char *) * (size_t)(ncfg + ndef + 1));
    if (mode == TG_PRIORITY_MODE_REPLACE) {
        for (int i = 0; i < ncfg; i++) result[rc++] = configured[i];
        /* configured owns the strings now; don't free them individually */
        free(configured); configured = NULL; ncfg = 0;
    } else if (mode == TG_PRIORITY_MODE_APPEND) {
        for (int i = 0; i < ndef; i++) result[rc++] = defaults[i];
        for (int i = 0; i < ncfg; i++) result[rc++] = configured[i];
        free(defaults); defaults = NULL; ndef = 0;
        free(configured); configured = NULL; ncfg = 0;
    } else { /* prepend */
        for (int i = 0; i < ncfg; i++) result[rc++] = configured[i];
        for (int i = 0; i < ndef; i++) result[rc++] = defaults[i];
        free(defaults); defaults = NULL; ndef = 0;
        free(configured); configured = NULL; ncfg = 0;
    }
    if (out_n) *out_n = rc;
    return result;
}

/* ── Telegram menu prioritization ─────────────────────────────────── */

void commands_prioritize_telegram_menu(cmd_entry_t *entries, int n,
                                       const char *raw_cfg_json) {
    if (!entries || n <= 0) return;
    int npri;
    char **pri = commands_telegram_effective_priority(raw_cfg_json, &npri);

    /* priority[name] = index (use pri directly; freed once at the end) */
    int *order = malloc(sizeof(int) * (size_t)n);
    for (int i = 0; i < n; i++) order[i] = i;

    /* stable sort by (tier, priority_index, original_pos) */
    for (int i = 1; i < n; i++) {
        int key = order[i];
        int j = i - 1;
        const char *kn = entries[key].name;
        int ktier = 1, kidx = n;
        for (int p = 0; p < npri; p++) if (strcmp(pri[p], kn) == 0) { ktier = 0; kidx = p; break; }
        while (j >= 0) {
            const char *jn = entries[order[j]].name;
            int jtier = 1, jidx = n;
            for (int p = 0; p < npri; p++) if (strcmp(pri[p], jn) == 0) { jtier = 0; jidx = p; break; }
            bool swap;
            if (ktier != jtier) swap = ktier < jtier;
            else if (kidx != jidx) swap = kidx < jidx;
            else swap = key < order[j];
            if (!swap) break;
            order[j + 1] = order[j];
            j--;
        }
        order[j + 1] = key;
    }

    /* reorder in place using a temp copy */
    cmd_entry_t *tmp = malloc(sizeof(cmd_entry_t) * (size_t)n);
    for (int i = 0; i < n; i++) {
        int src = order[i];
        cmd_entry_t *d = &tmp[i];
        memset(d, 0, sizeof(*d));
        memcpy(d->name, entries[src].name, CMD_NAME_LIMIT);
        d->description = entries[src].description ? strdup(entries[src].description) : NULL;
        d->key = entries[src].key ? strdup(entries[src].key) : NULL;
    }
    for (int i = 0; i < n; i++) {
        free(entries[i].description);
        free(entries[i].key);
        entries[i] = tmp[i];
    }
    free(tmp);
    free(order);
    for (int i = 0; i < npri; i++) free(pri[i]);
    free(pri);
}

/* ── Length clamp with collision avoidance ─────────────────────────── */

int commands_clamp_names(const cmd_entry_t *in, int n,
                         const char *const *reserved, int n_reserved,
                         cmd_entry_t *out, int out_cap, int *out_dropped) {
    int dropped = 0;
    int written = 0;
    if (out_dropped) *out_dropped = 0;
    if (!in || n <= 0 || !out || out_cap <= 0) {
        if (out_dropped) *out_dropped = n > 0 ? n : 0;
        return 0;
    }

    /* Build the used-name set (reserved first), mirroring Python's
     * `used = set(reserved)`. */
    char **used = malloc(sizeof(char *) * (size_t)(n_reserved + n + 1));
    int n_used = 0;
    for (int i = 0; i < n_reserved; i++)
        if (reserved[i] && *reserved[i]) used[n_used++] = strdup(reserved[i]);

    for (int i = 0; i < n; i++) {
        const cmd_entry_t *e = &in[i];
        const char *raw = e->name ? e->name : "";
        size_t rawlen = strlen(raw);
        char candidate[CMD_NAME_LIMIT + 1];
        memset(candidate, 0, sizeof(candidate));

        /* PoP: tools/skills_hub.py style length clamp. Faithful to
         * hermes_cli/commands.py:_clamp_command_names: renaming with a
         * 31-char prefix + 0-9 digit suffix happens ONLY when the name
         * exceeds the 32-char limit (truncation occurred). A <=32-char name
         * that merely collides with `used` is dropped, never renamed. */
        bool truncated = rawlen > CMD_NAME_LIMIT;
        strncpy(candidate, raw, CMD_NAME_LIMIT);
        candidate[CMD_NAME_LIMIT] = '\0';

        if (truncated) {
            bool collides = false;
            for (int u = 0; u < n_used; u++)
                if (strcmp(used[u], candidate) == 0) { collides = true; break; }
            if (collides) {
                char prefix[CMD_NAME_LIMIT];
                memcpy(prefix, candidate, CMD_NAME_LIMIT - 1);
                prefix[CMD_NAME_LIMIT - 1] = '\0';
                int chosen = -1;
                for (int d = 0; d < 10; d++) {
                    snprintf(candidate, sizeof(candidate), "%s%d", prefix, d);
                    bool inner = false;
                    for (int u = 0; u < n_used; u++)
                        if (strcmp(used[u], candidate) == 0) { inner = true; break; }
                    if (!inner) { chosen = d; break; }
                }
                if (chosen < 0) { dropped++; continue; } /* all 10 slots taken */
            }
        }

        /* Final dup check (covers reserved + earlier survivors, both
         * truncated-rename paths and <=32-char collisions). */
        bool dup = false;
        for (int u = 0; u < n_used; u++)
            if (strcmp(used[u], candidate) == 0) { dup = true; break; }
        if (dup) { dropped++; continue; }

        if (written >= out_cap) { dropped++; continue; }
        cmd_entry_t *o = &out[written++];
        memset(o, 0, sizeof(*o));
        memcpy(o->name, candidate, CMD_NAME_LIMIT);
        o->name[CMD_NAME_LIMIT] = '\0';
        o->description = e->description ? strdup(e->description) : NULL;
        o->key = e->key ? strdup(e->key) : NULL;
        used[n_used++] = strdup(candidate);
    }

    for (int u = 0; u < n_used; u++) free(used[u]);
    free(used);
    if (out_dropped) *out_dropped = dropped;
    return written;
}
