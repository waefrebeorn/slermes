/* port_turn_summary.c — C11 port of agent/turn_summary.py
 *
 * See port_turn_summary.h for the faithful-port contract.
 */

#include "port_turn_summary.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* Tool name -> (verb, singular noun, plural noun). */
typedef struct verb_group { const char *tool; const char *verb; const char *singular; const char *plural; } verb_group_t;
static const verb_group_t VERB_GROUPS[] = {
    {"write_file", "edited", "file", "files"},
    {"patch", "edited", "file", "files"},
    {"read_file", "read", "file", "files"},
    {"web_extract", "read", "page", "pages"},
    {"terminal", "ran", "command", "commands"},
    {"execute_code", "ran", "script", "scripts"},
    {"search_files", "searched", "path", "paths"},
    {"web_search", "searched the web", "time", "times"},
    {"session_search", "searched sessions", "time", "times"},
    {"browser_navigate", "browsed", "page", "pages"},
    {"skill_view", "read", "skill", "skills"},
    {"skill_manage", "updated", "skill", "skills"},
    {"skills_list", "listed skills", "time", "times"},
    {"todo", "updated", "task list", "task lists"},
    {"delegate_task", "delegated", "task", "tasks"},
    {"memory", "updated", "memory", "memories"},
};
static const size_t N_VERB_GROUPS = sizeof(VERB_GROUPS) / sizeof(VERB_GROUPS[0]);

static const char *EDIT_VERB = "edited";
static const char *VERB_PRIORITY[3] = {"edited", "read", "ran"};
static const char *DIFF_TOOLS[] = {"patch"};
static const size_t N_DIFF_TOOLS = 1;
#define SUMMARY_PREFIX_C "⋯"

/* ---- tally ---- */

turn_tally_t *turn_tally_new(void) {
    return calloc(1, sizeof(turn_tally_t));
}
void turn_tally_free(turn_tally_t *t) {
    if (!t) return;
    for (size_t i = 0; i < t->n_verbs; i++) {
        verb_entry_t *ve = &t->verbs[i];
        for (size_t j = 0; j < ve->n_nouns; j++) free(ve->nouns[j].plural);
        free(ve->nouns);
        free(ve->verb);
    }
    free(t->verbs);
    free(t);
}

/* PoP: turn_tally_total_tools @ agent/turn_summary.py:TurnTally.total_tools */
int turn_tally_total_tools(const turn_tally_t *t) {
    if (!t) return 0;
    int counted = 0;
    for (size_t i = 0; i < t->n_verbs; i++)
        for (size_t j = 0; j < t->verbs[i].n_nouns; j++)
            counted += t->verbs[i].nouns[j].count;
    return counted + t->other_tools;
}

static verb_entry_t *tally_find_verb(turn_tally_t *t, const char *verb) {
    for (size_t i = 0; i < t->n_verbs; i++)
        if (strcmp(t->verbs[i].verb, verb) == 0) return &t->verbs[i];
    if (t->n_verbs == t->cap_verbs) {
        t->cap_verbs = t->cap_verbs ? t->cap_verbs * 2 : 4;
        t->verbs = realloc(t->verbs, t->cap_verbs * sizeof(verb_entry_t));
    }
    verb_entry_t *ve = &t->verbs[t->n_verbs++];
    memset(ve, 0, sizeof(*ve));
    ve->verb = strdup(verb);
    return ve;
}
static void verb_add_noun(verb_entry_t *ve, const char *plural, int count) {
    for (size_t j = 0; j < ve->n_nouns; j++)
        if (strcmp(ve->nouns[j].plural, plural) == 0) {
            ve->nouns[j].count += count;
            return;
        }
    if (ve->n_nouns == ve->cap_nouns) {
        ve->cap_nouns = ve->cap_nouns ? ve->cap_nouns * 2 : 2;
        ve->nouns = realloc(ve->nouns, ve->cap_nouns * sizeof(noun_entry_t));
    }
    ve->nouns[ve->n_nouns].plural = strdup(plural);
    ve->nouns[ve->n_nouns].count = count;
    ve->n_nouns++;
}

/* ---- format_elapsed ---- */
/* PoP: format_elapsed @ agent/turn_summary.py:format_elapsed */
char *turn_summary_format_elapsed(double seconds) {
    if (seconds < 0) seconds = 0.0;
    char buf[32];
    if (seconds < 60.0) {
        snprintf(buf, sizeof buf, "%.1fs", seconds);
    } else {
        long total = llround(seconds);
        long minutes = total / 60, rest = total % 60;
        snprintf(buf, sizeof buf, "%ldm%02lds", minutes, rest);
    }
    return strdup(buf);
}

/* ---- _pluralize ---- */
/* PoP: _pluralize @ agent/turn_summary.py:_pluralize */
char *turn_summary_pluralize(int count, const char *plural_noun) {
    if (count == 1) {
        char *singular;
        size_t n = strlen(plural_noun);
        if (n >= 3 && strcmp(plural_noun + n - 3, "ies") == 0) {
            singular = malloc(n - 2);
            memcpy(singular, plural_noun, n - 3);
            singular[n - 3] = 'y'; singular[n - 2] = '\0';
        } else if (n >= 3 && strcmp(plural_noun + n - 3, "ses") == 0) {
            singular = malloc(n);
            memcpy(singular, plural_noun, n - 2);
            singular[n - 2] = '\0';
        } else if (n >= 1 && plural_noun[n - 1] == 's') {
            singular = malloc(n);
            memcpy(singular, plural_noun, n - 1);
            singular[n - 1] = '\0';
        } else {
            singular = strdup(plural_noun);
        }
        char *out = malloc(strlen(singular) + 8);
        sprintf(out, "1 %s", singular);
        free(singular);
        return out;
    }
    char *out = malloc(strlen(plural_noun) + 32);
    sprintf(out, "%d %s", count, plural_noun);
    return out;
}

/* ---- _count_diff_lines ---- */
/* PoP: _count_diff_lines @ agent/turn_summary.py:_count_diff_lines */
void turn_summary_count_diff_lines(const char *diff, int *out_added, int *out_removed) {
    int added = 0, removed = 0;
    if (!diff) { *out_added = 0; *out_removed = 0; return; }
    const char *p = diff;
    while (*p) {
        while (*p && *p != '\n') p++;
        if (!*p) break;
        const char *line = p + 1;
        /* line now points just past the newline; find its start/end */
        const char *line_start = line;
        const char *line_end = line;
        while (*line_end && *line_end != '\n') line_end++;
        /* classify by first char */
        if (line_end > line_start && (line_start[0] == '+' || line_start[0] == '-')) {
            if (line_start[1] == '+' || line_start[1] == '-') {
                /* +++ / --- header */
            } else if (line_start[0] == '+') added++;
            else if (line_start[0] == '-') removed++;
        }
        p = line_end;
    }
    *out_added = added;
    *out_removed = removed;
}

/* ---- _extract_line_deltas ---- */
/* PoP: _extract_line_deltas @ agent/turn_summary.py:_extract_line_deltas */
static bool extract_line_deltas(const char *tool_name, const json_t *result,
                                int *out_added, int *out_removed) {
    /* only diff-result tools */
    bool is_diff_tool = false;
    for (size_t i = 0; i < N_DIFF_TOOLS; i++)
        if (strcmp(DIFF_TOOLS[i], tool_name) == 0) { is_diff_tool = true; break; }
    if (!is_diff_tool) return false;

    const char *diff_text = NULL;
    if (result && result->type == JSON_STRING) {
        diff_text = result->str_val;
    } else if (result && result->type == JSON_OBJECT) {
        diff_text = json_get_str(result, "diff", NULL);
    }
    if (!diff_text || !*diff_text) return false;

    int added, removed;
    turn_summary_count_diff_lines(diff_text, &added, &removed);
    if (added == 0 && removed == 0) return false;
    *out_added = added;
    *out_removed = removed;
    return true;
}

/* ---- _ordered_verbs ---- */
/* PoP: _ordered_verbs @ agent/turn_summary.py:_ordered_verbs */
static const char **ordered_verbs(const turn_tally_t *t, size_t *out_n) {
    /* first-seen order */
    const char **seen = malloc(sizeof(char *) * (t->n_verbs ? t->n_verbs : 1));
    size_t ns = 0;
    for (size_t i = 0; i < t->n_verbs; i++) seen[ns++] = t->verbs[i].verb;
    const char **ranked = malloc(sizeof(char *) * (ns ? ns : 1));
    size_t nr = 0;
    for (size_t p = 0; p < 3; p++)
        for (size_t i = 0; i < ns; i++)
            if (strcmp(seen[i], VERB_PRIORITY[p]) == 0) ranked[nr++] = seen[i];
    for (size_t i = 0; i < ns; i++) {
        bool in_ranked = false;
        for (size_t r = 0; r < nr; r++) if (ranked[r] == seen[i]) { in_ranked = true; break; }
        if (!in_ranked) ranked[nr++] = seen[i];
    }
    free(seen);
    *out_n = nr;
    return ranked;
}

/* ---- format_turn_summary ---- */
/* PoP: format_turn_summary @ agent/turn_summary.py:format_turn_summary */
char *turn_summary_format(double elapsed_seconds, const turn_tally_t *tally,
                          int max_segments) {
    turn_tally_t empty;
    memset(&empty, 0, sizeof empty);
    if (!tally) tally = &empty;

    /* Build segments. */
    size_t cap_seg = (tally->n_verbs + 1) * 4 + 4;
    char **segments = malloc(sizeof(char *) * cap_seg);
    size_t n_seg = 0;

    size_t n_ranked = 0;
    const char **ranked = ordered_verbs(tally, &n_ranked);
    for (size_t r = 0; r < n_ranked; r++) {
        const char *verb = ranked[r];
        verb_entry_t *ve = NULL;
        for (size_t i = 0; i < tally->n_verbs; i++)
            if (strcmp(tally->verbs[i].verb, verb) == 0) { ve = &tally->verbs[i]; break; }
        if (!ve) continue;
        /* parts: plural -> count */
        char **parts = malloc(sizeof(char *) * (ve->n_nouns ? ve->n_nouns : 1));
        size_t n_parts = 0;
        for (size_t j = 0; j < ve->n_nouns; j++)
            if (ve->nouns[j].count > 0)
                parts[n_parts++] = turn_summary_pluralize(ve->nouns[j].count, ve->nouns[j].plural);
        if (!n_parts) { free(parts); continue; }
        /* join parts with ", " */
        size_t joincap = 1;
        for (size_t j = 0; j < n_parts; j++) joincap += strlen(parts[j]) + 2;
        char *joined = malloc(joincap);
        joined[0] = '\0';
        for (size_t j = 0; j < n_parts; j++) {
            strcat(joined, parts[j]);
            if (j + 1 < n_parts) strcat(joined, ", ");
        }
        size_t segcap = strlen(verb) + strlen(joined) + 32;
        char *segment = malloc(segcap);
        snprintf(segment, segcap, "%s %s", verb, joined);
        if (strcmp(verb, EDIT_VERB) == 0 && tally->has_line_deltas)
            sprintf(segment + strlen(segment), " +%d -%d", tally->lines_added, tally->lines_removed);
        for (size_t j = 0; j < n_parts; j++) free(parts[j]);
        free(parts);
        free(joined);
        segments[n_seg++] = segment;
    }

    if (tally->other_tools) {
        char *pl = turn_summary_pluralize(tally->other_tools, "tools");
        size_t cap = strlen(pl) + 16;
        char *seg = malloc(cap);
        snprintf(seg, cap, "called %s", pl);
        free(pl);
        segments[n_seg++] = seg;
    }

    if (!n_seg && turn_tally_total_tools(tally) == 0 && elapsed_seconds < TURN_SUMMARY_MIN_TOOLLESS) {
        free(ranked);
        for (size_t i = 0; i < n_seg; i++) free(segments[i]);
        free(segments);
        return strdup("");
    }

    if (max_segments > 0 && (long)n_seg > max_segments) {
        long hidden = (long)n_seg - max_segments;
        for (size_t i = max_segments; i < n_seg; i++) free(segments[i]);
        n_seg = max_segments;
        char *more = malloc(32);
        snprintf(more, 32, "+%ld more", hidden);
        segments[n_seg++] = more;
    }

    char *elapsed = turn_summary_format_elapsed(elapsed_seconds);
    /* assemble */
    size_t total = strlen(SUMMARY_PREFIX_C) + 1; /* " ⋯ " */
    total += strlen(elapsed);
    for (size_t i = 0; i < n_seg; i++) total += strlen(segments[i]) + 4; /* " · " (UTF-8) */
    char *out = malloc(total + 1);
    snprintf(out, total + 1, "%s ", SUMMARY_PREFIX_C);
    size_t pos = strlen(out);
    memcpy(out + pos, elapsed, strlen(elapsed) + 1);
    pos += strlen(elapsed);
    for (size_t i = 0; i < n_seg; i++) {
        memcpy(out + pos, " · ", 4);
        pos += 4;
        memcpy(out + pos, segments[i], strlen(segments[i]) + 1);
        pos += strlen(segments[i]);
    }
    free(elapsed);
    for (size_t i = 0; i < n_seg; i++) free(segments[i]);
    free(segments);
    free(ranked);
    return out;
}

/* ---- format_token_flow ---- */
/* PoP: format_token_flow @ agent/turn_summary.py:format_token_flow */
char *turn_summary_format_token_flow(const json_t *output_tokens, const char *arrow) {
    if (!output_tokens) return strdup("");
    long count;
    if (output_tokens->type == JSON_NUMBER) count = (long)output_tokens->num_val;
    else if (output_tokens->type == JSON_STRING) count = strtol(output_tokens->str_val, NULL, 10);
    else return strdup("");
    if (count <= 0) return strdup("");
    const char *arr = arrow ? arrow : "↓";
    char buf[64];
    if (count < 1000) snprintf(buf, sizeof buf, "%s %ld tok", arr, count);
    else if (count < 1000000) snprintf(buf, sizeof buf, "%s %.1fk tok", arr, count / 1000.0);
    else snprintf(buf, sizeof buf, "%s %.1fM tok", arr, count / 1000000.0);
    return strdup(buf);
}

/* ---- collector ---- */
struct turn_summary_collector {
    turn_tally_t *tally;
};

/* PoP: turn_summary_collector_new @ agent/turn_summary.py:TurnSummaryCollector.__init__ */
turn_summary_collector_t *turn_summary_collector_new(void) {
    turn_summary_collector_t *c = calloc(1, sizeof(*c));
    c->tally = turn_tally_new();
    return c;
}
void turn_summary_collector_free(turn_summary_collector_t *c) {
    if (!c) return;
    turn_tally_free(c->tally);
    free(c);
}

/* PoP: turn_summary_collector_begin @ agent/turn_summary.py:TurnSummaryCollector.begin */
void turn_summary_collector_begin(turn_summary_collector_t *c) {
    turn_tally_free(c->tally);
    c->tally = turn_tally_new();
}

/* PoP: turn_summary_collector_record @ agent/turn_summary.py:TurnSummaryCollector.record_tool */
void turn_summary_collector_record(turn_summary_collector_t *c,
                                   const char *tool_name,
                                   const json_t *result,
                                   bool is_error) {
    if (!tool_name || !*tool_name || is_error) return;
    if (tool_name[0] == '_') return; /* internal/pseudo tool */

    const verb_group_t *grp = NULL;
    for (size_t i = 0; i < N_VERB_GROUPS; i++)
        if (strcmp(VERB_GROUPS[i].tool, tool_name) == 0) { grp = &VERB_GROUPS[i]; break; }
    if (!grp) {
        c->tally->other_tools++;
        return;
    }
    verb_entry_t *ve = tally_find_verb(c->tally, grp->verb);
    verb_add_noun(ve, grp->plural, 1);

    if (strcmp(grp->verb, EDIT_VERB) == 0) {
        int added = 0, removed = 0;
        if (extract_line_deltas(tool_name, result, &added, &removed)) {
            c->tally->lines_added += added;
            c->tally->lines_removed += removed;
            c->tally->has_line_deltas = true;
        }
    }
}

/* PoP: TurnSummaryCollector.tally @ agent/turn_summary.py:TurnSummaryCollector.tally */
/* PoP: turn_summary_collector_tally @ agent/turn_summary.py:TurnSummaryCollector.tally */
const turn_tally_t *turn_summary_collector_tally(const turn_summary_collector_t *c) {
    return c->tally;
}

/* PoP: turn_summary_collector_render @ agent/turn_summary.py:TurnSummaryCollector.render */
char *turn_summary_collector_render(const turn_summary_collector_t *c, double elapsed_seconds) {
    return turn_summary_format(elapsed_seconds, c->tally, TURN_SUMMARY_MAX_SEGMENTS);
}
