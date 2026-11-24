/*
 * setup_agent_import.c — C home for upstream Hermes hermes_cli/setup_agent_import.py.
 *
 * PoP: w17_setupagent_0fd570_constants @ hermes_cli/setup_agent_import.py:IMPORT_SOURCES
 * PoP: w17_setupagent_0fd570_import_argv @ hermes_cli/setup_agent_import.py:_import_child
 * PoP: w17_setupagent_0fd570_migrate_hint @ hermes_cli/setup_agent_import.py:migrate_from_agents
 * PoP: w17_setupagent_0fd570_found_context @ hermes_cli/setup_agent_import.py:offer_agent_import
 *
 * The table is mirrored verbatim; argv/hint/context are the pure string
 * computations embedded in the otherwise side-effecting child-process paths.
 */
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "noc_w17_setupagent_0fd570.h"

static char *dupstr(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char *p = (char *)malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, s, n + 1);
    return p;
}

/* --- tiny string buffer (never a helper named `dup`) --- */
typedef struct { char *s; size_t len, cap; } sbuf;
static void sb_add(sbuf *b, const char *s) {
    if (!s) return;
    size_t n = strlen(s);
    if (b->len + n + 1 > b->cap) {
        size_t cap = b->cap ? b->cap : 64;
        while (cap < b->len + n + 1) cap *= 2;
        char *p = (char *)realloc(b->s, cap);
        if (!p) return;
        b->s = p; b->cap = cap;
    }
    memcpy(b->s + b->len, s, n + 1);
    b->len += n;
}
static char *sb_take(sbuf *b) {
    char *s = b->s ? b->s : dupstr("");
    b->s = NULL; b->len = b->cap = 0;
    return s;
}

typedef struct {
    const char *name;
    const char *label;
    const char *const *dirs;
    size_t ndirs;
    const char *const *extra_args;
    size_t nextra;
    const char *const *exports;
    size_t nexports;
} import_source_t;

static const char *const OC_DIRS[] = { ".openclaw", ".clawdbot", ".moltbot" };
static const char *const OC_EXTRA[] = { "--migrate-secrets" };
static const char *const CC_DIRS[] = { ".claude" };
static const char *const COD_DIRS[] = { ".codex" };
static const char *const CUR_DIRS[] = { ".cursor" };
static const char *const GROK_DIRS[] = {
    "Library/Application Support/Grok Bot", "/Applications/Grok Bot.app",
    "Applications/Grok Bot.app", ".config/Grok Bot", "AppData/Roaming/Grok Bot",
    "AppData/Local/Programs/Grok Bot" };
static const char *const GROK_EXP[] = { "Downloads/grok-bot-export*" };
static const char *const OB_DIRS[] = {
    "Library/Application Support/OpenBot", ".config/OpenBot",
    "AppData/Roaming/OpenBot", "OpenBot/Agents" };

/* IMPORT_SOURCES — verbatim from hermes_cli/setup_agent_import.py. */
static const import_source_t IMPORT_SOURCES[] = {
    { "openclaw",   "OpenClaw",   OC_DIRS, 3,  OC_EXTRA, 1, NULL, 0 },
    { "claude-code","Claude Code",CC_DIRS, 1,  NULL, 0, NULL, 0 },
    { "codex",      "Codex CLI",  COD_DIRS,1,  NULL, 0, NULL, 0 },
    { "cursor",     "Cursor",     CUR_DIRS,1,  NULL, 0, NULL, 0 },
    { "grok-bot",   "Grok Bot",   GROK_DIRS,6, NULL, 0, GROK_EXP, 1 },
    { "openbot",    "OpenBot",    OB_DIRS, 4,  NULL, 0, NULL, 0 },
};
static const size_t SOURCES_N = sizeof(IMPORT_SOURCES) / sizeof(IMPORT_SOURCES[0]);
static const char *PLUGIN = "agent-importers";

static json_t *str_array(const char *const *items, size_t n) {
    json_t *a = json_array();
    for (size_t i = 0; i < n; i++) json_append(a, json_string(items[i]));
    return a;
}

json_t *w17_setupagent_0fd570_constants(void) {
    json_t *out = json_object();
    json_set(out, "PLUGIN", json_string(PLUGIN));
    json_t *sources = json_array();
    for (size_t i = 0; i < SOURCES_N; i++) {
        const import_source_t *s = &IMPORT_SOURCES[i];
        json_t *o = json_object();
        json_set(o, "name", json_string(s->name));
        json_set(o, "label", json_string(s->label));
        json_set(o, "dirs", str_array(s->dirs, s->ndirs));
        json_set(o, "extra_args", str_array(s->extra_args, s->nextra));
        json_set(o, "exports", str_array(s->exports, s->nexports));
        json_append(sources, o);
    }
    json_set(out, "SOURCES", sources);
    return out;
}

static const import_source_t *find_source(const char *name) {
    if (!name) return NULL;
    for (size_t i = 0; i < SOURCES_N; i++)
        if (!strcmp(IMPORT_SOURCES[i].name, name)) return &IMPORT_SOURCES[i];
    return NULL;
}

json_t *w17_setupagent_0fd570_import_argv(const char *executable, const char *source,
                                          json_t *extra_args, json_t *selector, bool from_setup) {
    json_t *argv = json_array();
    json_append(argv, json_string(executable ? executable : ""));
    json_append(argv, json_string("-P"));
    json_append(argv, json_string("-m"));
    json_append(argv, json_string("hermes_cli.main"));
    if (json_is_array(selector))
        for (size_t i = 0; i < json_len(selector); i++) {
            json_t *v = json_get(selector, i);
            json_append(argv, json_string((v && v->type == JSON_STRING && v->str_val) ? v->str_val : ""));
        }
    json_append(argv, json_string("import-agent"));
    json_append(argv, json_string(source ? source : ""));
    const import_source_t *s = find_source(source);
    if (from_setup && s)
        for (size_t i = 0; i < s->nextra; i++) json_append(argv, json_string(s->extra_args[i]));
    if (json_is_array(extra_args))
        for (size_t i = 0; i < json_len(extra_args); i++) {
            json_t *v = json_get(extra_args, i);
            json_append(argv, json_string((v && v->type == JSON_STRING && v->str_val) ? v->str_val : ""));
        }
    if (from_setup) json_append(argv, json_string("--from-setup"));
    return argv;
}

char *w17_setupagent_0fd570_migrate_hint(json_t *found_labels, bool show_found) {
    sbuf b = {0};
    if (show_found) {
        sb_add(&b, "Found: ");
        if (json_is_array(found_labels))
            for (size_t i = 0; i < json_len(found_labels); i++) {
                if (i) sb_add(&b, ", ");
                json_t *v = json_get(found_labels, i);
                sb_add(&b, (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "");
            }
        sb_add(&b, ". ");
    } else {
        sb_add(&b, "No other agent setup found on this computer. ");
    }
    sb_add(&b, "Import one with: ");
    for (size_t i = 0; i < SOURCES_N; i++) {
        if (i) sb_add(&b, ", ");
        sb_add(&b, "hermes migrate ");
        sb_add(&b, IMPORT_SOURCES[i].name);
    }
    return sb_take(&b);
}

/* Windows Path str() emits backslashes; convert '/' to '\' then replace the
 * first occurrence of the (normalised) user home with '~'. */
static char *win_path(const char *p) {
    if (!p) return dupstr("");
    size_t n = strlen(p);
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    for (size_t i = 0; i < n; i++) out[i] = (p[i] == '/') ? '\\' : p[i];
    out[n] = '\0';
    return out;
}

char *w17_setupagent_0fd570_found_context(const char *user_home, json_t *found) {
    char *home = win_path(user_home);
    sbuf b = {0};
    sb_add(&b, "Found on this computer: ");
    if (json_is_array(found)) {
        for (size_t i = 0; i < json_len(found); i++) {
            if (i) sb_add(&b, ", ");
            json_t *e = json_get(found, i);
            const char *label = json_get_str(e, "label", "");
            const char *raw = json_get_str(e, "path", "");
            char *path = win_path(raw);
            bool replaced = false;
            if (home && *home && path) {
                char *at = strstr(path, home);
                if (at) {
                    *at = '\0';
                    sbuf tmp = {0};
                    sb_add(&tmp, path);
                    sb_add(&tmp, "~");
                    sb_add(&tmp, at + strlen(home));
                    free(path);
                    path = sb_take(&tmp);
                    replaced = true;
                }
            }
            (void)replaced;
            sb_add(&b, label);
            sb_add(&b, " (");
            sb_add(&b, path ? path : "");
            sb_add(&b, ")");
            free(path);
        }
    }
    sb_add(&b, ".");
    free(home);
    return sb_take(&b);
}
