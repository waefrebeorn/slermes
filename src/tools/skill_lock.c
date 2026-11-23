/*
 * skill_lock.c — C home for upstream Hermes tools/skill_lock.py.
 * PoP: w6_skilllock_842861_skill_refusal @ tools/skill_lock.py:skill_refusal
 * PoP: w6_skilllock_842861_unquoted_newlines_to_semicolons @ tools/skill_lock.py:_unquoted_newlines_to_semicolons
 * PoP: w6_skilllock_842861_segments @ tools/skill_lock.py:_segments
 * PoP: w6_skilllock_842861_operands @ tools/skill_lock.py:_operands
 * PoP: w6_skilllock_842861_target_directory @ tools/skill_lock.py:_target_directory
 * PoP: w6_skilllock_842861_find_exec_commands @ tools/skill_lock.py:_find_exec_commands
 */
#include "noc_w6_skilllock_842861.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int all_in(const char *s, const char *set) {
    if (!s || !*s) return 0;
    for (const char *p = s; *p; p++)
        if (!strchr(set, *p)) return 0;
    return 1;
}

char *w6_skilllock_842861_skill_refusal(const char *name) {
    char *out;
    size_t n;
    if (!name) name = "";
    n = strlen("Skill '") + strlen(name) + strlen(
        "' is locked by the user, so the agent cannot change it: skill_manage, "
        "write_file, patch and terminal all refuse. Don't retry or work around it (no copying it "
        "under another name); tell the user what you would change. They can unlock it with "
        "`hermes curator unlock ") + strlen(name) + strlen(
        "` or the lock switch in Desktop.") + 1;
    out = (char *)malloc(n);
    snprintf(out, n,
        "Skill '%s' is locked by the user, so the agent cannot change it: skill_manage, "
        "write_file, patch and terminal all refuse. Don't retry or work around it (no copying it "
        "under another name); tell the user what you would change. They can unlock it with "
        "`hermes curator unlock %s` or the lock switch in Desktop.", name, name);
    return out;
}

char *w6_skilllock_842861_unquoted_newlines_to_semicolons(const char *command) {
    size_t n;
    char *out;
    char quote = 0;
    size_t j = 0;
    if (!command) command = "";
    n = strlen(command);
    out = (char *)malloc(n + 1);
    for (size_t i = 0; i < n; i++) {
        char c = command[i];
        if (quote) {
            if (c == quote) quote = 0;
        } else if (c == '\'' || c == '"') {
            quote = c;
        } else if (c == '\n') {
            c = ';';
        }
        out[j++] = c;
    }
    out[j] = '\0';
    return out;
}

struct json_t *w6_skilllock_842861_segments(const struct json_t *tokens) {
    json_t *res = json_array();
    json_t *argv = json_array();
    json_t *targets = json_array();
    size_t n = (tokens && tokens->type == JSON_ARRAY) ? json_len(tokens) : 0;
    size_t i = 0;
    while (i < n) {
        const json_t *tv = json_get(tokens, i);
        const char *tok = (tv && tv->type == JSON_STRING) ? tv->str_val : "";
        i++;
        if (strchr(tok, '>') && all_in(tok, "&>|0123456789")) {
            if (!tok[strlen(tok) - 1] || tok[strlen(tok) - 1] != '&') {
                const json_t *nv = i < n ? json_get(tokens, i) : NULL;
                const char *target = (nv && nv->type == JSON_STRING) ? nv->str_val : "";
                if (i < n) i++;
                if (*target && target[0] != '&' && strcmp(target, "/dev/null"))
                    json_append(targets, json_string(target));
            }
            continue;
        }
        if (!strcmp(tok, "<") || !strcmp(tok, "<<") || !strcmp(tok, "<<<") || !strcmp(tok, "<<-")) {
            if (i < n) i++;   /* input file / heredoc delimiter: read, never written */
            continue;
        }
        if (*tok && all_in(tok, ";&|()")) {
            json_t *seg = json_array();
            json_append(seg, json_copy(argv));
            json_append(seg, json_copy(targets));
            json_append(seg, json_string(tok));
            json_append(res, seg);
            json_free(argv); json_free(targets);
            argv = json_array(); targets = json_array();
            continue;
        }
        json_append(argv, json_string(tok));
    }
    {
        json_t *seg = json_array();
        json_append(seg, json_copy(argv));
        json_append(seg, json_copy(targets));
        json_append(seg, json_string(""));
        json_append(res, seg);
    }
    json_free(argv); json_free(targets);
    return res;
}

struct json_t *w6_skilllock_842861_operands(const struct json_t *args) {
    json_t *res = json_array();
    size_t n = (args && args->type == JSON_ARRAY) ? json_len(args) : 0;
    for (size_t i = 0; i < n; i++) {
        const json_t *v = json_get(args, i);
        const char *a = (v && v->type == JSON_STRING) ? v->str_val : NULL;
        if (a && *a && a[0] != '-') json_append(res, json_string(a));
    }
    return res;
}

struct json_t *w6_skilllock_842861_target_directory(const struct json_t *args) {
    json_t *res = json_array();
    json_t *rest = json_array();
    const char *dir = NULL;
    size_t n = (args && args->type == JSON_ARRAY) ? json_len(args) : 0;
    for (size_t i = 0; i < n && !dir; i++) {
        const json_t *v = json_get(args, i);
        const char *a = (v && v->type == JSON_STRING) ? v->str_val : "";
        if (!strncmp(a, "--target-directory=", 19)) {
            dir = a + 19;
            for (size_t j = 0; j < n; j++) if (j != i) {
                const json_t *x = json_get(args, j);
                json_append(rest, json_copy(x));
            }
        } else if ((!strcmp(a, "-t") || !strcmp(a, "--target-directory")) && i + 1 < n) {
            const json_t *x = json_get(args, i + 1);
            dir = (x && x->type == JSON_STRING) ? x->str_val : "";
            for (size_t j = 0; j < n; j++) if (j != i && j != i + 1) {
                const json_t *y = json_get(args, j);
                json_append(rest, json_copy(y));
            }
        } else if (!strncmp(a, "-t", 2) && strlen(a) > 2 && strncmp(a, "--", 2)) {
            dir = a + 2;
            for (size_t j = 0; j < n; j++) if (j != i) {
                const json_t *x = json_get(args, j);
                json_append(rest, json_copy(x));
            }
        }
    }
    if (!dir) for (size_t j = 0; j < n; j++) json_append(rest, json_copy(json_get(args, j)));
    json_append(res, dir ? json_string(dir) : json_null());
    json_append(res, rest);
    return res;
}

struct json_t *w6_skilllock_842861_find_exec_commands(const struct json_t *args) {
    json_t *res = json_array();
    json_t *group = NULL;
    size_t n = (args && args->type == JSON_ARRAY) ? json_len(args) : 0;
    for (size_t i = 0; i < n; i++) {
        const json_t *v = json_get(args, i);
        const char *a = (v && v->type == JSON_STRING) ? v->str_val : "";
        if (!strcmp(a, "-exec") || !strcmp(a, "-execdir") || !strcmp(a, "-ok") || !strcmp(a, "-okdir")) {
            if (group) json_free(group);
            group = json_array();
        } else if (group && (!strcmp(a, ";") || !strcmp(a, "+"))) {
            json_append(res, group);
            group = NULL;
        } else if (group) {
            json_append(group, json_copy(v));
        }
    }
    if (group && json_len(group)) json_append(res, group);
    else if (group) json_free(group);
    return res;
}

