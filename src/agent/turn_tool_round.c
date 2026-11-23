/* turn_tool_round.c — C home for upstream Hermes agent/turn_tool_round.py.
 *
 * PoP: w7_turntoolro_tool_round_facts @ agent/turn_tool_round.py:stage_tool_call_message
 * The upstream stage_tool_call_message classifies each tool batch against the module
 * constants _HOUSEKEEPING_TOOLS / _LINE_IS_REPLY_TOOLS and drops a bare bracketed
 * scaffold marker (agent.conversation_loop._STALE_MARKER_RE =
 * re.compile(r"^\[[A-Za-z_][A-Za-z0-9_.-]*\]$")) from turn content. I/O-driven
 * verdicts live in the turn loop; this module owns the exact pure decisions.
 */
#include "noc_w7_turntoolro_99622f.h"
#include "hermes_json.h"
#include <string.h>

static const char *const HOUSEKEEPING_TOOLS[] = {
    "memory", "todo_list", "skill_manage", "session_search"
};
/* _LINE_IS_REPLY_TOOLS = _HOUSEKEEPING_TOOLS | {"setup_beat"} */
static const char *const LINE_IS_REPLY_TOOLS[] = {
    "memory", "todo_list", "skill_manage", "session_search", "setup_beat"
};

static bool in_set(const char *name, const char *const *set, size_t n) {
    for (size_t i = 0; i < n; i++)
        if (name && !strcmp(name, set[i])) return true;
    return false;
}

/* Python str.strip() for the ASCII whitespace used by protocol markers. */
static const char *strip_ascii(const char *s) {
    if (!s) return s;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r' ||
           *s == '\f' || *s == '\v') s++;
    return s;
}

/* fullmatch of ^\[[A-Za-z_][A-Za-z0-9_.-]*\]$ on the stripped content. */
static bool is_stale_marker(const char *content) {
    const char *s = strip_ascii(content);
    if (!s) return false;
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\n' ||
                     s[n - 1] == '\r' || s[n - 1] == '\f' || s[n - 1] == '\v')) n--;
    if (n < 3 || s[0] != '[' || s[n - 1] != ']') return false;
    const char *p = s + 1;
    if (!((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || *p == '_')) return false;
    for (p++; p < s + n - 1; p++) {
        if (!((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
              (*p >= '0' && *p <= '9') || *p == '_' || *p == '.' || *p == '-')) return false;
    }
    return true;
}

void w7_turntoolro_tool_round_facts(const json_t *tool_names, const char *content,
                                    bool *all_housekeeping, bool *line_is_reply,
                                    bool *drop_marker) {
    bool hk = true, reply = true;
    size_t count = (tool_names && tool_names->type == JSON_ARRAY) ? json_len(tool_names) : 0;
    for (size_t i = 0; i < count; i++) {
        const json_t *tc = json_get(tool_names, i);
        const char *name = (tc && tc->type == JSON_STRING) ? tc->str_val : "";
        if (!in_set(name, HOUSEKEEPING_TOOLS,
                    sizeof(HOUSEKEEPING_TOOLS) / sizeof(HOUSEKEEPING_TOOLS[0]))) hk = false;
        if (!in_set(name, LINE_IS_REPLY_TOOLS,
                    sizeof(LINE_IS_REPLY_TOOLS) / sizeof(LINE_IS_REPLY_TOOLS[0]))) reply = false;
    }
    if (all_housekeeping) *all_housekeeping = hk;
    if (line_is_reply) *line_is_reply = reply;
    if (drop_marker) *drop_marker = (count > 0) && is_stale_marker(content);
}
