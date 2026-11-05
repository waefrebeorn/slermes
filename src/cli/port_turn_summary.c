/*
 * port_turn_summary.c — Port of Python: agent/turn_summary.py
 *
 * Renders the CLI's one-line per-turn accounting:
 *   ⋯ 12.4s · edited 2 files +18 -3 · read 4 files · ran 3 commands
 *
 * Pure display logic. The Python collector keeps insertion order in a dict;
 * this port keeps a bounded array in the same order, so the rendered segment
 * order matches (edited, read, ran, then first-seen).
 */

#include "port_turn_summary.h"
#include "hermes_json.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SUMMARY_PREFIX "\xe2\x8b\xaf"      /* UTF-8 for U+22EF (⋯) */
#define MIN_TOOLLESS_SECONDS 2.0
#define MAX_SEGMENTS 4
#define EDIT_VERB "edited"

static const struct { const char *tool, *verb, *plural; } VERB_GROUPS[] = {
    { "write_file",      "edited",             "files" },
    { "patch",           "edited",             "files" },
    { "read_file",       "read",               "files" },
    { "web_extract",     "read",               "pages" },
    { "terminal",        "ran",                "commands" },
    { "execute_code",    "ran",                "scripts" },
    { "search_files",    "searched",           "paths" },
    { "web_search",      "searched the web",   "times" },
    { "session_search",  "searched sessions",  "times" },
    { "browser_navigate","browsed",            "pages" },
    { "skill_view",      "read",               "skills" },
    { "skill_manage",    "updated",            "skills" },
    { "skills_list",     "listed skills",      "times" },
    { "todo_list",       "updated",            "task lists" },
    { "delegate_task",   "delegated",          "tasks" },
    { "memory",          "updated",            "memories" },
};

static const char *const VERB_PRIORITY[] = { "edited", "read", "ran" };

static char *dup_str(const char *s)
{
    if (!s) return NULL;
    size_t n = strlen(s);
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

static char *fmt_alloc(const char *fmt, ...)
{
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n < 0) return NULL;
    char *out = (char *)malloc((size_t)n + 1);
    if (!out) return NULL;
    memcpy(out, buf, (size_t)n + 1);
    return out;
}

void ts_tally_clear(ts_tally_t *tally)
{
    if (!tally) return;
    for (int i = 0; i < tally->n_verbs; i++) {
        free(tally->verbs[i].verb);
        for (int j = 0; j < tally->verbs[i].n_nouns; j++)
            free(tally->verbs[i].nouns[j]);
    }
    memset(tally, 0, sizeof(*tally));
}

void ts_tally_init(ts_tally_t *tally)
{
    if (tally) memset(tally, 0, sizeof(*tally));
}

int ts_tally_total_tools(const ts_tally_t *tally)
{
    if (!tally) return 0;
    int total = tally->other_tools;
    for (int i = 0; i < tally->n_verbs; i++)
        for (int j = 0; j < tally->verbs[i].n_nouns; j++)
            total += tally->verbs[i].counts[j];
    return total;
}

static ts_verb_group_t *find_or_add_verb(ts_tally_t *tally, const char *verb)
{
    for (int i = 0; i < tally->n_verbs; i++)
        if (strcmp(tally->verbs[i].verb, verb) == 0) return &tally->verbs[i];
    if (tally->n_verbs >= TS_MAX_VERBS) return NULL;
    ts_verb_group_t *g = &tally->verbs[tally->n_verbs++];
    memset(g, 0, sizeof(*g));
    g->verb = dup_str(verb);
    return g;
}

static void bump_noun(ts_verb_group_t *g, const char *plural)
{
    for (int i = 0; i < g->n_nouns; i++) {
        if (strcmp(g->nouns[i], plural) == 0) { g->counts[i] += 1; return; }
    }
    if (g->n_nouns >= TS_MAX_NOUNS) return;
    g->nouns[g->n_nouns] = dup_str(plural);
    g->counts[g->n_nouns] = 1;
    g->n_nouns++;
}

/* PoP: ts_extract_line_deltas @ agent/turn_summary.py:_extract_line_deltas */
int ts_extract_line_deltas(const char *tool_name, const char *result,
                           int *added, int *removed)
{
    if (!tool_name || strcmp(tool_name, "patch") != 0) return 0;
    if (!result) return 0;

    json_t *payload = NULL;
    int owns = 0;
    const char *p = result;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (*p == '{') {
        payload = json_parse(p, NULL);
        owns = 1;
        if (!payload) return 0;
    }
    if (!payload || payload->type != JSON_OBJECT) {
        if (owns) json_free(payload);
        return 0;
    }

    const char *diff = json_get_str(payload, "diff", NULL);
    if (!diff) { if (owns) json_free(payload); return 0; }
    /* copy before freeing the document */
    char *d = dup_str(diff);
    if (owns) json_free(payload);
    if (!d) return 0;

    int a = 0, r = 0;
    const char *line = d;
    while (*line) {
        const char *end = line;
        while (*end && *end != '\n' && *end != '\r') end++;
        size_t len = (size_t)(end - line);
        int is_header = (len >= 3 && strncmp(line, "+++", 3) == 0) ||
                        (len >= 3 && strncmp(line, "---", 3) == 0);
        if (!is_header) {
            if (len >= 1 && line[0] == '+') a++;
            else if (len >= 1 && line[0] == '-') r++;
        }
        if (*end == '\r') { end++; if (*end == '\n') end++; }
        else if (*end == '\n') end++;
        line = end;
    }
    free(d);
    if (a == 0 && r == 0) return 0;
    if (added) *added = a;
    if (removed) *removed = r;
    return 1;
}

/* PoP: ts_tally_record @ agent/turn_summary.py:record_tool */
void ts_tally_record(ts_tally_t *tally, const char *tool_name,
                     const char *result_json, int is_error)
{
    if (!tally || !tool_name || is_error || tool_name[0] == '_') return;

    const char *verb = NULL, *plural = NULL;
    for (size_t i = 0; i < sizeof(VERB_GROUPS) / sizeof(VERB_GROUPS[0]); i++) {
        if (strcmp(tool_name, VERB_GROUPS[i].tool) == 0) {
            verb = VERB_GROUPS[i].verb;
            plural = VERB_GROUPS[i].plural;
            break;
        }
    }
    if (!verb) { tally->other_tools += 1; return; }

    ts_verb_group_t *g = find_or_add_verb(tally, verb);
    if (!g) return;
    bump_noun(g, plural);

    if (strcmp(verb, EDIT_VERB) == 0) {
        int added = 0, removed = 0;
        if (ts_extract_line_deltas(tool_name, result_json, &added, &removed)) {
            tally->lines_added += added;
            tally->lines_removed += removed;
            tally->has_line_deltas = 1;
        }
    }
}

/* PoP: ts_format_elapsed @ agent/turn_summary.py:format_elapsed */
char *ts_format_elapsed(double seconds)
{
    if (seconds < 0.0) seconds = 0.0;
    if (seconds < 60.0) return fmt_alloc("%.1fs", seconds);
    long rounded = lround(seconds);
    long minutes = rounded / 60;
    long rest = rounded % 60;
    return fmt_alloc("%ldm%02lds", minutes, rest);
}

/* PoP: ts_pluralize @ agent/turn_summary.py:_pluralize */
char *ts_pluralize(int count, const char *plural_noun)
{
    if (!plural_noun) plural_noun = "";
    if (count != 1) return fmt_alloc("%d %s", count, plural_noun);

    struct { const char *suffix, *tail; } rules[] = {
        { "ies", "y" }, { "ses", "s" }, { "s", "" },
    };
    size_t len = strlen(plural_noun);
    for (size_t i = 0; i < sizeof(rules) / sizeof(rules[0]); i++) {
        size_t slen = strlen(rules[i].suffix);
        if (len >= slen && strcmp(plural_noun + len - slen, rules[i].suffix) == 0) {
            char stem[128];
            size_t keep = len - slen;
            if (keep >= sizeof(stem)) keep = sizeof(stem) - 1;
            memcpy(stem, plural_noun, keep);
            stem[keep] = '\0';
            return fmt_alloc("1 %s%s", stem, rules[i].tail);
        }
    }
    return fmt_alloc("1 %s", plural_noun);
}

/* PoP: ts_format_token_flow @ agent/turn_summary.py:format_token_flow */
char *ts_format_token_flow(long output_tokens, const char *arrow)
{
    const char *a = (arrow && *arrow) ? arrow : "\xe2\x86\x93";   /* ↓ */
    if (output_tokens <= 0) return dup_str("");
    if (output_tokens < 1000) return fmt_alloc("%s %ld tok", a, output_tokens);
    if (output_tokens < 1000000)
        return fmt_alloc("%s %.1fk tok", a, (double)output_tokens / 1000.0);
    return fmt_alloc("%s %.1fM tok", a, (double)output_tokens / 1000000.0);
}

static int verb_rank(const char *verb)
{
    for (size_t i = 0; i < sizeof(VERB_PRIORITY) / sizeof(VERB_PRIORITY[0]); i++)
        if (strcmp(verb, VERB_PRIORITY[i]) == 0) return (int)i;
    return -1;
}

/* PoP: ts_format_summary @ agent/turn_summary.py:format_turn_summary */
char *ts_format_summary(double elapsed_seconds, const ts_tally_t *tally,
                        int max_segments)
{
    ts_tally_t empty;
    if (!tally) { ts_tally_init(&empty); tally = &empty; }

    /* ordered = priority verbs present, then the rest in insertion order */
    int order[TS_MAX_VERBS], n_order = 0;
    for (size_t p = 0; p < sizeof(VERB_PRIORITY) / sizeof(VERB_PRIORITY[0]); p++)
        for (int i = 0; i < tally->n_verbs; i++)
            if (strcmp(tally->verbs[i].verb, VERB_PRIORITY[p]) == 0)
                order[n_order++] = i;
    for (int i = 0; i < tally->n_verbs; i++)
        if (verb_rank(tally->verbs[i].verb) < 0) order[n_order++] = i;

    char **segments = (char **)calloc((size_t)n_order + 2, sizeof(char *));
    int n_seg = 0;
    if (!segments) return NULL;

    for (int k = 0; k < n_order; k++) {
        const ts_verb_group_t *g = &tally->verbs[order[k]];
        char parts[512];
        size_t used = 0;
        parts[0] = '\0';
        for (int j = 0; j < g->n_nouns; j++) {
            if (g->counts[j] == 0) continue;
            char *part = ts_pluralize(g->counts[j], g->nouns[j]);
            if (!part) continue;
            used += (size_t)snprintf(parts + used, sizeof(parts) - used, "%s%s",
                                     used ? ", " : "", part);
            free(part);
            if (used >= sizeof(parts)) break;
        }
        if (!parts[0]) continue;
        char seg[768];
        snprintf(seg, sizeof(seg), "%s %s", g->verb, parts);
        if (strcmp(g->verb, EDIT_VERB) == 0 && tally->has_line_deltas) {
            snprintf(seg + strlen(seg), sizeof(seg) - strlen(seg), " +%d -%d",
                     tally->lines_added, tally->lines_removed);
        }
        segments[n_seg++] = dup_str(seg);
    }

    if (tally->other_tools) {
        char *p = ts_pluralize(tally->other_tools, "tools");
        segments[n_seg] = fmt_alloc("called %s", p ? p : "");
        free(p);
        n_seg++;
    }

    if (n_seg == 0 && ts_tally_total_tools(tally) == 0 &&
        elapsed_seconds < MIN_TOOLLESS_SECONDS) {
        for (int i = 0; i < n_seg; i++) free(segments[i]);
        free(segments);
        return dup_str("");
    }

    int limit = (max_segments <= 0) ? 0 : max_segments;
    char hidden[64] = "";
    if (limit > 0 && n_seg > limit) {
        snprintf(hidden, sizeof(hidden), "+%d more", n_seg - limit);
        n_seg = limit;
    }

    char *elapsed = ts_format_elapsed(elapsed_seconds);
    size_t cap = 64 + (elapsed ? strlen(elapsed) : 0);
    for (int i = 0; i < n_seg; i++) cap += strlen(segments[i]) + 4;
    cap += strlen(hidden) + 8;
    char *out = (char *)malloc(cap);
    if (out) {
        size_t o = 0;
        o += (size_t)snprintf(out + o, cap - o, "%s %s", SUMMARY_PREFIX,
                              elapsed ? elapsed : "");
        for (int i = 0; i < n_seg; i++)
            o += (size_t)snprintf(out + o, cap - o, " \xc2\xb7 %s", segments[i]);
        if (hidden[0])
            o += (size_t)snprintf(out + o, cap - o, " \xc2\xb7 %s", hidden);
    }
    for (int i = 0; i < n_seg; i++) free(segments[i]);
    free(segments);
    free(elapsed);
    return out;
}
