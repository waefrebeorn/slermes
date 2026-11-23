/*
 * cli_info_mixin.c — C port of the pure helpers in upstream Hermes
 * hermes_cli/cli_info_mixin.py.
 *
 * PoP: w12_cliinfomi_ea6329_tool_progress_label @ hermes_cli/cli_info_mixin.py:_tool_progress_label
 * PoP: w12_cliinfomi_ea6329_reload_mcp_choices @ hermes_cli/cli_info_mixin.py:_reload_mcp_choices
 * PoP: w12_cliinfomi_ea6329_help_section_title @ hermes_cli/cli_info_mixin.py:_help_section_title
 * PoP: w12_cliinfomi_ea6329_skill_line @ hermes_cli/cli_info_mixin.py:_skill_line
 * PoP: w12_cliinfomi_ea6329_toolset_map @ hermes_cli/cli_info_mixin.py:_toolset_map
 * PoP: w12_cliinfomi_ea6329_ascii_box @ hermes_cli/cli_info_mixin.py:_ascii_box
 *
 * t() resolves to the identity key and _Colors is injected as deterministic
 * sentinel strings (<DIM>/<YELLOW>/<GREEN>/<BOLD>/<RESET>) by the oracle.
 */
#include "noc_w12_cliinfomi_ea6329.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LINEBUF 8192
#define C_DIM    "<DIM>"
#define C_YELLOW "<YELLOW>"
#define C_GREEN  "<GREEN>"
#define C_BOLD   "<BOLD>"
#define C_RESET  "<RESET>"

static json_t *jget(const json_t *o, const char *k) {
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}
static const char *jstr(const json_t *o, const char *k, const char *d) {
    return (o && o->type == JSON_OBJECT) ? json_get_str(o, k, d) : d;
}
static void push(json_t *arr, const char *s) { json_append(arr, json_string(s)); }

struct json_t *w12_cliinfomi_ea6329_tool_progress_label(const char *mode) {
    const char *m = mode ? mode : "";
    const char *style = NULL;
    if (!strcmp(m, "off")) style = C_DIM;
    else if (!strcmp(m, "new")) style = C_YELLOW;
    else if (!strcmp(m, "all")) style = C_GREEN;
    else if (!strcmp(m, "verbose")) style = C_BOLD C_GREEN;
    if (!style) return json_string("");
    char key[256], detail[256], out[LINEBUF];
    snprintf(key, sizeof(key), "cli.verbose.label_%s", m);
    snprintf(detail, sizeof(detail), "cli.verbose.detail_%s", m);
    snprintf(out, sizeof(out), "%s%s%s \xe2\x80\x94 %s", style, key, C_RESET, detail);
    return json_string(out);
}

struct json_t *w12_cliinfomi_ea6329_reload_mcp_choices(void) {
    static const char *const rows[3][3] = {
        { "once",   "cli.reload_mcp.choice_once",   "cli.reload_mcp.choice_once_hint" },
        { "always", "cli.reload_mcp.choice_always", "cli.reload_mcp.choice_always_hint" },
        { "cancel", "cli.reload_mcp.choice_cancel", "cli.reload_mcp.choice_cancel_hint" },
    };
    json_t *out = json_array();
    for (int i = 0; i < 3; i++) {
        json_t *row = json_array();
        for (int j = 0; j < 3; j++) push(row, rows[i][j]);
        json_append(out, row);
    }
    return out;
}

struct json_t *w12_cliinfomi_ea6329_help_section_title(const char *category) {
    const char *c = category ? category : "";
    static const char *const cats[] = {
        "Session", "Configuration", "Tools & Skills", "Info", "Exit", "Context",
        "Background & Automation", NULL
    };
    static const char *const slugs[] = {
        "session", "configuration", "tools_skills", "info", "exit", "context",
        "background_automation", NULL
    };
    for (int i = 0; cats[i]; i++) {
        if (!strcmp(c, cats[i])) {
            char out[256];
            snprintf(out, sizeof(out), "cli.help.section_%s", slugs[i]);
            return json_string(out);
        }
    }
    return json_string(c);
}

struct json_t *w12_cliinfomi_ea6329_skill_line(const struct json_t *item) {
    const char *nm = jstr(item, "name", "");
    const char *desc = jstr(item, "description", "");
    char out[LINEBUF];
    if (*desc) snprintf(out, sizeof(out), "    - %s: %s", nm, desc);
    else snprintf(out, sizeof(out), "    - %s", nm);
    return json_string(out);
}

/* dict.setdefault: only writes when the key is ABSENT (a present null stays null). */
static void setdefault_str(json_t *o, const char *key, const json_t *val) {
    if (json_obj_get(o, key) != NULL) return;
    json_set(o, key, val ? json_copy(val) : json_null());
}

struct json_t *w12_cliinfomi_ea6329_toolset_map(const struct json_t *tools,
        const struct json_t *availability, const struct json_t *toolset_of) {
    json_t *out = json_object();
    if (tools && tools->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(tools); i++) {
            json_t *fn = jget(json_get(tools, i), "function");
            const char *nm = jstr(fn, "name", "");
            json_set(out, nm, jget(toolset_of, nm) ? json_copy(jget(toolset_of, nm)) : json_null());
        }
    }
    json_t *unavail = jget(availability, "unavailable_toolsets");
    if (unavail && unavail->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(unavail); i++) {
            json_t *item = json_get(unavail, i);
            json_t *idv = jget(item, "id");
            if (!idv) idv = jget(item, "name");
            if (!idv) idv = json_string("");
            json_t *names = jget(item, "tools");
            if (names && names->type == JSON_ARRAY) {
                for (size_t j = 0; j < json_len(names); j++) {
                    const char *nm = json_string_value(json_get(names, j));
                    setdefault_str(out, nm ? nm : "", idv);
                }
            }
            if (!jget(item, "id") && !jget(item, "name")) json_free(idv);
        }
    }
    return out;
}

struct json_t *w12_cliinfomi_ea6329_ascii_box(const char *title, int width) {
    json_t *out = json_array();
    if (width < 0) width = 0;
    char rule[512], mid[512];
    size_t i = 0;
    rule[i++] = '+';
    for (int k = 0; k < width && i + 2 < sizeof(rule); k++) rule[i++] = '-';
    rule[i++] = '+'; rule[i] = '\0';
    const char *t = title ? title : "";
    size_t pad = (size_t)width > strlen(t) ? (size_t)width - strlen(t) : 0;
    size_t left = pad / 2, right = pad - left;
    size_t j = 0;
    mid[j++] = '|';
    for (size_t k = 0; k < left && j + 2 < sizeof(mid); k++) mid[j++] = ' ';
    for (const char *p = t; *p && j + 2 < sizeof(mid); p++) mid[j++] = *p;
    for (size_t k = 0; k < right && j + 2 < sizeof(mid); k++) mid[j++] = ' ';
    mid[j++] = '|'; mid[j] = '\0';
    push(out, rule); push(out, mid); push(out, rule);
    return out;
}
