/*
 * main_profile_recovery.c -- real C11 port of the stale-`active_profile`
 * escape-hatch gate of upstream Hermes hermes_cli/main_profile_recovery.py.
 *
 * PoP: w21_mainprofil_8ddd4c_command_argv @ hermes_cli/main_profile_recovery.py:is_stale_profile_recovery_command
 * PoP: w21_mainprofil_8ddd4c_is_stale_profile_recovery_command @ hermes_cli/main_profile_recovery.py:is_stale_profile_recovery_command
 *
 * `command_argv` is lifted out of hermes_cli._parser (it is called from here,
 * not re-derived): the value-taking top-level flags are the set
 * `top_level_value_flag_sets()` reports for the LIVE parser plus the
 * pre-argparse inherited `--profile/-p`.  `_uninstall_keeps_data` builds the
 * real `uninstall` subparser and parses `["uninstall", *args]` under
 * `parse_known_args`; the option surface below is that subparser
 * (--full/--gui/--data mutually exclusive, --gui-summary, -y/--yes, --dry-run,
 * --desktop-userdata VALUE) with argparse's exact abbreviation rules.
 */
#include "hermes_json.h"
#include "noc_w21_mainprofil_8ddd4c.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* required-value flags from build_top_level_parser() + the pre-argparse pair */
static const char *const MP8_VALUE_FLAGS[] = {
    "-z", "--oneshot", "-m", "--model", "--provider", "--reasoning", "-t",
    "--toolsets", "-r", "--resume", "-s", "--skills", "--usage-file", "--in",
    "--cpu-profile", "-c", "--continue", "--profile", "-p", NULL
};

static int mp8_is_value_flag(const char *token) {
    int i;
    for (i = 0; MP8_VALUE_FLAGS[i]; i++) {
        if (!strcmp(token, MP8_VALUE_FLAGS[i])) return 1;
    }
    return 0;
}

/* argv as a C vector of borrowed strings (fixture array; non-strings skipped). */
static size_t mp8_argv_view(const json_t *argv, const char **vec, size_t cap) {
    size_t i, n = 0;
    if (!argv || argv->type != JSON_ARRAY) return 0;
    for (i = 0; i < argv->c.count && n < cap; i++) {
        json_t *v = argv->c.items[i];
        if (v && v->type == JSON_STRING && v->str_val) vec[n++] = v->str_val;
    }
    return n;
}

json_t *w21_mainprofil_8ddd4c_command_argv(const json_t *argv) {
    const char *vec[64];
    size_t n = mp8_argv_view(argv, vec, 64);
    json_t *out = json_array();
    size_t i = 0;

    while (i < n) {
        const char *token = vec[i];
        if (!strcmp(token, "--")) {
            for (i = i + 1; i < n; i++) json_append(out, json_string(vec[i]));
            return out;
        }
        if (token[0] != '-') {
            for (; i < n; i++) json_append(out, json_string(vec[i]));
            return out;
        }
        if (!strchr(token, '=') && mp8_is_value_flag(token) && i + 1 < n) {
            i += 2;
        } else {
            i += 1;
        }
    }
    return out;
}

/* --- the uninstall subparser --------------------------------------------- */

typedef enum { MP8_STORE_TRUE, MP8_VALUE, MP8_HELP } mp8_opt_kind;

typedef struct {
    const char *name;
    mp8_opt_kind kind;
} mp8_opt_row;

static const mp8_opt_row MP8_UNINSTALL_OPTS[] = {
    {"--full",             MP8_STORE_TRUE},
    {"--gui",              MP8_STORE_TRUE},
    {"--data",             MP8_STORE_TRUE},
    {"--gui-summary",      MP8_STORE_TRUE},
    {"--yes",              MP8_STORE_TRUE},
    {"--dry-run",          MP8_STORE_TRUE},
    {"--desktop-userdata", MP8_VALUE},
    {"--help",             MP8_HELP},
    {NULL,                 MP8_STORE_TRUE}
};

typedef enum { MP8_PARSE_OK, MP8_PARSE_HELP, MP8_PARSE_ERROR } mp8_parse_result;

/*
 * `parse_known_args(["uninstall", *args])` over that surface.  `keeps_data` is
 * `not (ns.data or ns.full)` on success; the caller maps HELP to True and
 * a parse error (SystemExit != 0) to False, exactly as `_uninstall_keeps_data` does.
 */
static mp8_parse_result mp8_parse_uninstall(const char *const *args, size_t n, int *keeps_data) {
    int yes = 0, full = 0, gui = 0, data = 0;
    size_t i;

    *keeps_data = 0;
    for (i = 0; i < n; i++) {
        const char *token = args[i];
        if (!strcmp(token, "--")) break;              /* the rest are positionals */
        if (token[0] == '-' && token[1] == '-') {
            const char *eq = strchr(token, '=');
            size_t name_len = eq ? (size_t)(eq - token) : strlen(token);
            const mp8_opt_row *match = NULL;
            int matches = 0, k;

            for (k = 0; MP8_UNINSTALL_OPTS[k].name; k++) {
                const char *name = MP8_UNINSTALL_OPTS[k].name;
                if (strlen(name) == name_len && !strncmp(name, token, name_len)) {
                    match = &MP8_UNINSTALL_OPTS[k];    /* exact match wins outright */
                    matches = 1;
                    break;
                }
            }
            if (!match) {
                for (k = 0; MP8_UNINSTALL_OPTS[k].name; k++) {
                    if (!strncmp(MP8_UNINSTALL_OPTS[k].name, token, name_len)) {
                        match = &MP8_UNINSTALL_OPTS[k];
                        matches++;
                    }
                }
            }
            if (matches > 1) return MP8_PARSE_ERROR;   /* "ambiguous option" */
            if (!match) continue;                      /* unknown -> extras */
            if (match->kind == MP8_HELP) {
                if (eq) return MP8_PARSE_ERROR;
                return MP8_PARSE_HELP;
            }
            if (match->kind == MP8_VALUE) {
                if (!eq && i + 1 >= n) return MP8_PARSE_ERROR;   /* expected one argument */
                if (!eq) i++;
                continue;
            }
            if (eq) return MP8_PARSE_ERROR;            /* "ignored explicit argument" */
            if (!strcmp(match->name, "--full")) {
                full = 1;
            } else if (!strcmp(match->name, "--gui")) {
                gui = 1;
            } else if (!strcmp(match->name, "--data")) {
                data = 1;
            } else if (!strcmp(match->name, "--yes")) {
                yes = 1;
            }
            if (full + gui + data > 1) return MP8_PARSE_ERROR;   /* mutually exclusive */
        } else if (token[0] == '-' && token[1] != '\0') {
            if (!strcmp(token, "-h")) return MP8_PARSE_HELP;
            if (!strcmp(token, "-y")) yes = 1;
            /* any other short flag is an unrecognized extra, not an error */
        }
        /* a bare positional is an unrecognized extra too */
    }
    (void)yes;
    *keeps_data = !(data || full);
    return MP8_PARSE_OK;
}

static int mp8_casefold_is_default(const char *s) {
    static const char WORD[] = "default";
    size_t i;
    if (!s) return 0;
    for (i = 0; WORD[i]; i++) {
        char c = s[i];
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        if (c != WORD[i]) return 0;
    }
    return s[i] == '\0';
}

int w21_mainprofil_8ddd4c_is_stale_profile_recovery_command(const json_t *argv) {
    const char *vec[64];
    size_t n = mp8_argv_view(argv, vec, 64);
    const char *command[64];
    size_t cn = 0, i, start = 0;

    /* command_argv(): drop leading top-level flags and their values */
    while (start < n) {
        const char *token = vec[start];
        if (!strcmp(token, "--")) {
            start++;
            break;
        }
        if (token[0] != '-') break;
        if (!strchr(token, '=') && mp8_is_value_flag(token) && start + 1 < n) {
            start += 2;
        } else {
            start += 1;
        }
    }
    for (i = start; i < n && cn < 64; i++) command[cn++] = vec[i];

    if (cn >= 2 && !strcmp(command[0], "profile") && !strcmp(command[1], "list")) return 1;
    if (cn >= 2 && !strcmp(command[0], "profile") && !strcmp(command[1], "use")) {
        return (cn > 2 && mp8_casefold_is_default(command[2])) ? 1 : 0;
    }
    if (cn >= 1 && !strcmp(command[0], "uninstall")) {
        int keeps_data = 0;
        mp8_parse_result res = mp8_parse_uninstall(command + 1, cn - 1, &keeps_data);
        if (res == MP8_PARSE_HELP) return 1;           /* SystemExit(0) */
        if (res == MP8_PARSE_ERROR) return 0;          /* SystemExit(2) */
        return keeps_data ? 1 : 0;
    }
    return 0;
}
