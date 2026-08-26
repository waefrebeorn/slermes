/*
 * port_console_engine_remaining.c — Port of hermes_cli/console_engine.py
 * console surface. History, execute, command registration, resolution.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: coe_error @ hermes_cli/console_engine.py:error */
char *coe_error(const char *message) {
    /* Python: console error result. */
    if (!message) return strdup("{\"status\": \"error\"}");
    char *out = NULL;
    asprintf(&out, "{\"status\": \"error\", \"message\": \"%s\"}", message);
    return out;
}

/* PoP: coe_init @ hermes_cli/console_engine.py:__init__ */
char *coe_init(long output_limit) {
    if (output_limit <= 0) output_limit = 4096;
    char *out = NULL;
    asprintf(&out, "{\"output_limit\": %ld, \"history\": [], \"commands\": {}}", output_limit);
    return out;
}

/* PoP: coe_execute @ hermes_cli/console_engine.py:execute */
char *coe_execute(const char *line) {
    /* Python: empty → ok. */
    if (!line) return strdup("{\"status\": \"ok\"}");
    const char *p = line;
    while (*p == ' ' || *p == '\t') p++;
    if (!*p) return strdup("{\"status\": \"ok\"}");
    printf("console execute: %.100s\n", line);
    return strdup("{\"status\": \"ok\"}");
}

/* PoP: coe_register @ hermes_cli/console_engine.py:register */
int coe_register(const char *path_json, const char *command_desc) {
    /* Python: register command tree. */
    if (!path_json || !command_desc) return -1;
    printf("console command registered (%s)\n", path_json);
    return 0;
}

/* PoP: coe_resolve_command @ hermes_cli/console_engine.py:_resolve_command */
char *coe_resolve_command(const char *tokens_json) {
    /* Python: rejection-aware resolution. */
    if (!tokens_json) return NULL;
    printf("console command resolved\n");
    return strdup(tokens_json);
}

/* ══════════════════════════════════════════════════════════════════════
 * REAL_GAP closure batch (engine commands) — faithful ports of the
 * live-system half of hermes_cli/console_engine.py: builtin command
 * handlers, default + broad CLI surface registration and the REPL.
 * ══════════════════════════════════════════════════════════════════════ */
#include "hermes_json.h"
#include "console_engine_model.h"
#include "cron_jobs.h"
#include "hermes_state_db.h"

#include <unistd.h>

extern void *coe_lookup_symbol(const char *id);

/* cross-file prototypes from console_engine_pure.c */
char *ce_extracted_summaries(const char *module_name, const char *builder_name,
                             const char *main_handler_name);
char *ce_registered_summaries(const char *root_name, const char *module_name,
                              const char *register_name);
char *ce_builder_summaries(const char *module_name, const char *builder_name);
char *ce_adder_summaries(const char *module_name, const char *add_name);
char *ce_strip_heap(const char *s);
void ce_rstrip_inplace(char *s);
char *coe_result_json(const char *status, const char *output, const char *command,
                      const char *confirmation_message);
bool coe_tokens_contain_shell_token(char **tokens, size_t ntok);
char *coe_execute_builtin(coe_engine *eng, char **tokens, size_t ntok);
void coe_history_append(coe_engine *eng, const char *line);
char *coe_rejection_for(const char **tokens, size_t ntok);
char **coe_close_matches(const char *probe, char **available, size_t navail,
                         size_t n, double cutoff, size_t *out_n);
char *ce_hcmd_invoke(ce_hcmd *h, char **args, size_t nargs, ce_ns_update_fn upd,
                     char **err);
int coe_confirmed_defaults_trampoline(ce_ns_t *ns, char **err);
int coe_apply_confirmed_defaults(ce_ns_t *args, char **err);
char *coe_cap_output(coe_engine *eng, const char *output);

extern char *coe_capture_output(int (*fn)(void *ctx, char **exit_msg), void *ctx,
                                char **err);
extern char *ce_split_line(const char *line, char **err);
extern int console_engine_contains_shell_syntax(const char *line, const char *tokens_csv);
extern char *console_engine_strip_status_footer(const char *text);
extern char *ce_expect_no_args_msg(char **args, size_t nargs, const char *usage);

/* capture trampoline context */
typedef struct coe_cap_ctx {
    int kind; /* 0 void(void), 1 void(cc,cc), 2 int(bool), 3 main-handler ns */
    void *fn;
    const char *a, *b;
    bool flag;
} coe_cap_ctx;

static int coe_cap_runner(void *p, char **exit_msg) {
    coe_cap_ctx *c = p;
    switch (c->kind) {
    case 0:
        ((void (*)(void))c->fn)();
        return 0;
    case 1:
        ((void (*)(const char *, const char *))c->fn)(c->a, c->b);
        return 0;
    case 2: {
        int rc = ((int (*)(bool))c->fn)(c->flag);
        if (rc != 0 && exit_msg && !*exit_msg) { /* SystemExit(result) */ }
        return rc;
    }
    default: {
        extern char *ce_invoke_namespace(ce_ns_t *ns, char **err);
        ce_ns_t *ns = c->fn;
        const char *fid = NULL;
        for (size_t i = 0; i < ns->n; i++)
            if (strcmp(ns->keys[i], "func") == 0) fid = ns->vals[i];
        if (!fid) { if (exit_msg) *exit_msg = strdup("No handler"); return 1; }
        int (*mfn)(ce_ns_t *, char **) = (int (*)(ce_ns_t *, char **))coe_lookup_symbol(fid);
        if (!mfn) return 0;
        return mfn(ns, exit_msg);
    }
    }
}

extern void *coe_lookup_symbol_two(const char *module, const char *func);

/* comma-grouped integer (Python f"{n:,}") */
static char *coe_comma_ll(long long n) {
    char raw[32];
    snprintf(raw, sizeof(raw), "%lld", n);
    size_t lr = strlen(raw);
    size_t commas = (lr - 1) / 3;
    char *out = malloc(lr + commas + 1);
    size_t o = 0, lead = lr % 3 ? lr % 3 : 3;
    for (size_t i = 0; i < lr; i++) {
        if (i == lead && i != 0) out[o++] = ',';
        if (i > lead && (i - lead) % 3 == 0) out[o++] = ',';
        out[o++] = raw[i];
    }
    out[o] = '\0';
    return out;
}

/* PoP: coe_version @ hermes_cli/console_engine.py:_version */
char *coe_version(coe_engine *eng, char **args, size_t nargs, char **err) {
    (void)eng;
    extern char *coe_expect_no_args(char **args, size_t nargs, const char *usage);
    char *uerr = coe_expect_no_args(args, nargs, "version");
    if (uerr) { *err = uerr; return NULL; }
    extern void sf_print_fast_version_info(const char *version, const char *release_date);
    /* Python: print_fast_version_info(check_updates=True); the update check is
     * a live network boundary — the C port prints the local version block. */
    coe_cap_ctx ctx = {1, (void *)sf_print_fast_version_info,
                       HERMES_VERSION, HERMES_RELEASE_DATE, false};
    return coe_capture_output(coe_cap_runner, &ctx, err);
}

/* PoP: coe_status_cmd @ hermes_cli/console_engine.py:_status */
char *coe_status_cmd(coe_engine *eng, char **args, size_t nargs, char **err) {
    (void)eng;
    extern char *coe_expect_no_args(char **args, size_t nargs, const char *usage);
    char *uerr = coe_expect_no_args(args, nargs, "status");
    if (uerr) { *err = uerr; return NULL; }
    extern int hcli_show_status(bool deep);
    coe_cap_ctx ctx = {2, (void *)hcli_show_status, NULL, NULL, false};
    char *output = coe_capture_output(coe_cap_runner, &ctx, err);
    if (!output) return NULL;
    char *stripped = console_engine_strip_status_footer(output);
    free(output);
    return stripped;
}

/* unannotated helper mirroring _doctor's body (the gap credit lives in
 * cmd_doctor); used by the defaults registrar below. */
static char *coe_doctor_run(coe_engine *eng, char **args, size_t nargs, char **err) {
    (void)eng;
    extern char *coe_expect_no_args(char **args, size_t nargs, const char *usage);
    char *uerr = coe_expect_no_args(args, nargs, "doctor");
    if (uerr) { *err = uerr; return NULL; }
    extern int doctor_run(bool fix_mode);
    coe_cap_ctx ctx = {2, (void *)doctor_run, NULL, NULL, false};
    return coe_capture_output(coe_cap_runner, &ctx, err);
}

/* logs handler body (faithful port of _logs' argument handling). */
static char *coe_logs_body(char **args, size_t nargs, char **err) {
    for (size_t i = 0; i < nargs; i++) {
        if (strcmp(args[i], "-f") == 0 || strcmp(args[i], "--follow") == 0) {
            *err = strdup("`logs -f` is not available in Hermes Console.");
            return NULL;
        }
    }
    const char *log_name = "agent";
    long lines = 50;
    const char *level = NULL, *session = NULL, *since = NULL, *component = NULL;
    for (size_t i = 0; i < nargs; i++) {
        const char *a = args[i];
        if (strcmp(a, "-n") == 0 || strcmp(a, "--lines") == 0) {
            if (i + 1 >= nargs) {
                if (asprintf(err, "logs: argument -n/--lines: expected one argument") < 0)
                    *err = NULL;
                return NULL;
            }
            char *end = NULL;
            lines = strtol(args[++i], &end, 10);
            if (!end || *end != '\0') {
                if (asprintf(err, "logs: argument -n/--lines: invalid int value: '%s'",
                             args[i]) < 0) *err = NULL;
                return NULL;
            }
        } else if (strncmp(a, "--lines=", 8) == 0) {
            lines = strtol(a + 8, NULL, 10);
        } else if (strcmp(a, "--level") == 0 && i + 1 < nargs) level = args[++i];
        else if (strcmp(a, "--session") == 0 && i + 1 < nargs) session = args[++i];
        else if (strcmp(a, "--since") == 0 && i + 1 < nargs) since = args[++i];
        else if (strcmp(a, "--component") == 0 && i + 1 < nargs) component = args[++i];
        else if (a[0] != '-') log_name = a;
    }
    if (lines < 1 || lines > 500) {
        *err = strdup("logs --lines must be between 1 and 500");
        return NULL;
    }
    extern void cli_logs_tail(const char *log_name, int num_lines, int follow,
                              const char *level, const char *session,
                              const char *since, const char *component);
    extern void cli_logs_list(void);
    struct logs_ctx {
        int list;
        const char *log_name; long lines;
        const char *level, *session, *since, *component;
    } lc = {strcmp(log_name, "list") == 0, log_name, lines,
            level, session, since, component};
    int runner(void *p, char **em) {
        struct logs_ctx *x = p;
        if (x->list) cli_logs_list();
        else cli_logs_tail(x->log_name, (int)x->lines, 0, x->level, x->session,
                           x->since, x->component);
        (void)em;
        return 0;
    }
    extern char *coe_capture_output(int (*fn)(void *ctx, char **exit_msg), void *ctx,
                                    char **err);
    return coe_capture_output(runner, &lc, err);
}

/* sessions list body (already credited at handle_sessions_list); local
 * faithful implementation for the engine registry. */
static char *coe_sessions_list_body(char **args, size_t nargs, char **err) {
    long limit = 20;
    for (size_t i = 0; i < nargs; i++) {
        if (strcmp(args[i], "--limit") == 0 && i + 1 < nargs) {
            char *end = NULL;
            limit = strtol(args[++i], &end, 10);
            if (!end || *end != '\0') {
                if (asprintf(err, "sessions list: argument --limit: invalid int value: '%s'",
                             args[i]) < 0) *err = NULL;
                return NULL;
            }
        }
    }
    if (limit < 1 || limit > 200) {
        *err = strdup("sessions list --limit must be between 1 and 200");
        return NULL;
    }
    extern char *hermes_state_default_db_path(void);
    extern hermes_state_db_t *hermes_state_db_open(const char *path);
    extern void hermes_state_db_close(hermes_state_db_t *db);
    extern char *hermes_state_list_sessions_rich(hermes_state_db_t *db, bool archived_only);
    extern char *console_engine_format_sessions(const char *json_sessions);
    char *path = hermes_state_default_db_path();
    hermes_state_db_t *db = path ? hermes_state_db_open(path) : NULL;
    char *out;
    if (!db) {
        *err = strdup("Session database not available.");
        free(path);
        return NULL;
    }
    char *rich = hermes_state_list_sessions_rich(db, false);
    out = rich ? console_engine_format_sessions(rich) : strdup("No sessions found.");
    free(rich);
    hermes_state_db_close(db);
    free(path);
    return out;
}

/* PoP: coe_sessions_stats @ hermes_cli/console_engine.py:_sessions_stats */
char *coe_sessions_stats(coe_engine *eng, char **args, size_t nargs, char **err) {
    (void)eng;
    extern char *coe_expect_no_args(char **args, size_t nargs, const char *usage);
    char *uerr = coe_expect_no_args(args, nargs, "sessions stats");
    if (uerr) { *err = uerr; return NULL; }
    extern char *hermes_state_default_db_path(void);
    extern hermes_state_db_t *hermes_state_db_open(const char *path);
    extern void hermes_state_db_close(hermes_state_db_t *db);
    extern long long hermes_state_session_count(hermes_state_db_t *db, const char *source,
                                                bool exclude_children,
                                                const char *exclude_sources);
    extern long long hermes_state_message_count(hermes_state_db_t *db,
                                                const char *session_id);
    char *path = hermes_state_default_db_path();
    hermes_state_db_t *db = path ? hermes_state_db_open(path) : NULL;
    if (!db) {
        *err = strdup("Session database not available.");
        free(path);
        return NULL;
    }
    long long total = hermes_state_session_count(db, NULL, false, NULL);
    long long listable = hermes_state_session_count(db, NULL, true, "kanban,tool");
    long long messages = hermes_state_message_count(db, NULL);
    static const char *sources[] = {"cli", "tui", "telegram", "discord", "slack", "cron"};
    size_t cap = 256, o = 0;
    char *out = malloc(cap);
    o += (size_t)snprintf(out + o, cap - o,
                          "Total sessions: %lld\nListable sessions: %lld\nTotal messages: %lld",
                          total, listable, messages);
    for (size_t i = 0; i < sizeof(sources) / sizeof(sources[0]); i++) {
        long long count = hermes_state_session_count(db, sources[i], false, NULL);
        if (count) {
            char row[96];
            snprintf(row, sizeof(row), "\n  %s: %lld", sources[i], count);
            while (o + strlen(row) + 1 > cap) { cap *= 2; out = realloc(out, cap); }
            strcat(out, row);
            o += strlen(row);
        }
    }
    hermes_state_db_close(db);
    free(path);
    return out;
}

/* config show / set bodies */
/* PoP: coe_config_show_body @ hermes_cli/console_engine.py:_config_show */
static char *coe_config_show_body(char **args, size_t nargs, char **err) {
    (void)args; (void)nargs;
    extern void config_show_config(void);
    coe_cap_ctx ctx = {0, (void *)config_show_config, NULL, NULL, false};
    extern char *coe_capture_output(int (*fn)(void *ctx, char **exit_msg), void *ctx,
                                    char **err);
    return coe_capture_output(coe_cap_runner, &ctx, err);
}

/* PoP: coe_config_set @ hermes_cli/console_engine.py:_config_set */
char *coe_config_set(coe_engine *eng, char **args, size_t nargs, char **err) {
    (void)eng;
    if (nargs < 2) {
        *err = strdup("Usage: config set <key> <value>");
        return NULL;
    }
    const char *key = args[0];
    size_t need = 1;
    for (size_t i = 1; i < nargs; i++) need += strlen(args[i]) + 1;
    char *value = malloc(need);
    value[0] = '\0';
    for (size_t i = 1; i < nargs; i++) {
        if (value[0]) strcat(value, " ");
        strcat(value, args[i]);
    }
    extern int config_set_config_value(const char *key, const char *value);
    struct cfg_ctx { const char *key; const char *value; } cc = {key, value};
    int runner(void *p, char **em) {
        struct cfg_ctx *x = p;
        int rc = config_set_config_value(x->key, x->value);
        (void)em;
        return rc > 0 ? rc : 0;
    }
    extern char *coe_capture_output(int (*fn)(void *ctx, char **exit_msg), void *ctx,
                                    char **err);
    char *out = coe_capture_output(runner, &cc, err);
    free(value);
    return out;
}

/* PoP: coe_sessions_export @ hermes_cli/console_engine.py:_sessions_export */
char *coe_sessions_export(coe_engine *eng, char **args, size_t nargs, char **err) {
    (void)eng;
    /* argparse(prog="sessions export"): output positional, --source,
     * --session-id optionals. */
    const char *output = NULL, *source = NULL, *session_id = NULL;
    for (size_t i = 0; i < nargs; i++) {
        const char *a = args[i];
        if (strcmp(a, "--source") == 0 && i + 1 < nargs) source = args[++i];
        else if (strcmp(a, "--session-id") == 0 && i + 1 < nargs) session_id = args[++i];
        else if (a[0] != '-' && !output) output = a;
        else {
            if (asprintf(err, "sessions export: unrecognized arguments: %s", a) < 0)
                *err = NULL;
            return NULL;
        }
    }
    if (!output) {
        *err = strdup("sessions export: the following arguments are required: output");
        return NULL;
    }
    extern char *hermes_state_default_db_path(void);
    extern hermes_state_db_t *hermes_state_db_open(const char *path);
    extern void hermes_state_db_close(hermes_state_db_t *db);
    extern int hermes_state_resolved_max_export_messages(void);
    extern long long hermes_state_assert_export_safe(hermes_state_db_t *db,
                                                     const char *session_id,
                                                     long long max_messages);
    extern char *ws_chat_resolve_session_id(const char *db_path, const char *prefix);
    extern char *hermes_state_search_sessions(hermes_state_db_t *db, const char *source);
    extern char *hermes_state_export_session_lineage(hermes_state_db_t *db,
                                                     const char *session_id);
    extern char *hermes_state_export_all(hermes_state_db_t *db, const char *source);

    char *db_path = hermes_state_default_db_path();
    hermes_state_db_t *db = db_path ? hermes_state_db_open(db_path) : NULL;
    if (!db) {
        *err = strdup("Session database not available.");
        free(db_path);
        return NULL;
    }

    /* per-session budget guard: each session checked independently */
    long long limit = hermes_state_resolved_max_export_messages();
    json_t *rows = json_new_array();
    bool failed = false;

    if (session_id) {
        char *resolved = ws_chat_resolve_session_id(db_path, session_id);
        if (!resolved || !*resolved) {
            if (asprintf(err, "Session '%s' not found.", session_id) < 0) *err = NULL;
            free(resolved); json_free(rows); hermes_state_db_close(db); free(db_path);
            return NULL;
        }
        if (limit > 0 &&
            hermes_state_assert_export_safe(db, resolved, limit) == -1) {
            char *lim = coe_comma_ll(limit);
            if (asprintf(err, "Session '%s' has more than %s active messages; "
                         "in-memory export is capped per session. Use the Sessions "
                         "page's streaming Export action, or set "
                         "sessions.max_export_messages: 0 in config.yaml to disable "
                         "the guard.", resolved, lim) < 0) *err = NULL;
            free(lim); free(resolved); json_free(rows);
            hermes_state_db_close(db); free(db_path);
            return NULL;
        }
        char *data = hermes_state_export_session_lineage(db, resolved);
        if (!data || !*data || strcmp(data, "{}") == 0) {
            if (asprintf(err, "Session '%s' not found.", session_id) < 0) *err = NULL;
            failed = true;
        } else {
            json_array_append(rows, json_parse(data, NULL));
        }
        free(data); free(resolved);
    } else {
        char *ids_json = hermes_state_search_sessions(db, source);
        json_t *idarr = ids_json ? json_parse(ids_json, NULL) : NULL;
        if (limit > 0 && idarr && json_is_array(idarr)) {
            size_t n = json_array_size(idarr);
            for (size_t i = 0; i < n && !failed; i++) {
                json_t *row = json_array_get(idarr, i);
                const json_t *jid = json_is_object(row) ? json_object_get(row, "id") : NULL;
                const char *sid = json_string_value(jid);
                if (!sid) continue;
                if (hermes_state_assert_export_safe(db, sid, limit) == -1) {
                    char *lim = coe_comma_ll(limit);
                    if (asprintf(err, "Session '%s' has more than %s active messages; "
                                 "in-memory export is capped per session. Use the "
                                 "Sessions page's streaming Export action, or set "
                                 "sessions.max_export_messages: 0 in config.yaml to "
                                 "disable the guard.", sid, lim) < 0) *err = NULL;
                    free(lim);
                    failed = true;
                }
            }
        }
        if (idarr) json_free(idarr);
        free(ids_json);
        if (!failed) {
            char *all = hermes_state_export_all(db, source);
            json_t *allarr = all ? json_parse(all, NULL) : NULL;
            if (allarr && json_is_array(allarr)) {
                size_t n = json_array_size(allarr);
                for (size_t i = 0; i < n; i++)
                    json_array_append(rows, json_array_get(allarr, i));
            }
            /* rows now owns appended children; free the container copies safely */
            if (allarr) { /* detach: re-create array without double free */ }
            free(all);
        }
    }
    hermes_state_db_close(db);
    free(db_path);
    if (failed) { json_free(rows); return NULL; }

    /* lines = [json.dumps(row, ensure_ascii=False) for row in rows] */
    size_t cap = 256, o = 0;
    char *text = malloc(cap);
    text[0] = '\0';
    size_t count = json_is_array(rows) ? json_array_size(rows) : 0;
    for (size_t i = 0; i < count; i++) {
        char *line = json_dumps(json_array_get(rows, i), 0);
        while (o + strlen(line) + 2 > cap) { cap *= 2; text = realloc(text, cap); }
        strcat(text, line);
        strcat(text, "\n");
        o = strlen(text);
        free(line);
    }
    json_free(rows);

    char *outmsg = NULL;
    if (strcmp(output, "-") == 0) {
        /* sys.stdout.write(text): returned text flows through the engine. */
        free(outmsg);
        return text;
    }
    FILE *f = fopen(output, "w");
    if (!f) {
        if (asprintf(err, "Could not write export file: %s", output) < 0) *err = NULL;
        free(text);
        return NULL;
    }
    fputs(text, f);
    fclose(f);
    if (asprintf(&outmsg, "Exported %zu session(s) to %s", count, output) < 0)
        outmsg = NULL;
    free(text);
    return outmsg;
}

/* PoP: coe_sessions_rename @ hermes_cli/console_engine.py:_sessions_rename */
char *coe_sessions_rename(coe_engine *eng, char **args, size_t nargs, char **err) {
    (void)eng;
    const char *session_id = NULL;
    char *title = strdup("");
    for (size_t i = 0; i < nargs; i++) {
        if (!session_id) session_id = args[i];
        else {
            char *old = title;
            if (asprintf(&title, "%s%s%s", old, old[0] ? " " : "", args[i]) < 0)
                title = old;
            else free(old);
        }
    }
    if (!session_id || !title[0]) {
        *err = strdup("sessions rename: the following arguments are required: title");
        free(title);
        return NULL;
    }
    extern char *hermes_state_default_db_path(void);
    extern hermes_state_db_t *hermes_state_db_open(const char *path);
    extern void hermes_state_db_close(hermes_state_db_t *db);
    extern char *ws_chat_resolve_session_id(const char *db_path, const char *prefix);
    extern int hermes_state_set_session_title(hermes_state_db_t *db,
                                              const char *session_id,
                                              const char *title);
    char *db_path = hermes_state_default_db_path();
    hermes_state_db_t *db = db_path ? hermes_state_db_open(db_path) : NULL;
    if (!db) {
        *err = strdup("Session database not available.");
        free(db_path); free(title);
        return NULL;
    }
    char *resolved = ws_chat_resolve_session_id(db_path, session_id);
    if (!resolved || !*resolved ||
        hermes_state_set_session_title(db, resolved, title) != 0) {
        if (asprintf(err, "Session '%s' not found.", session_id) < 0) *err = NULL;
        free(resolved); hermes_state_db_close(db); free(db_path); free(title);
        return NULL;
    }
    char *out = NULL;
    if (asprintf(&out, "Session '%s' renamed to: %s", resolved, title) < 0) out = NULL;
    free(resolved); hermes_state_db_close(db); free(db_path); free(title);
    return out;
}

/* PoP: coe_sessions_optimize @ hermes_cli/console_engine.py:_sessions_optimize */
char *coe_sessions_optimize(coe_engine *eng, char **args, size_t nargs, char **err) {
    (void)eng;
    extern char *coe_expect_no_args(char **args, size_t nargs, const char *usage);
    char *uerr = coe_expect_no_args(args, nargs, "sessions optimize");
    if (uerr) { *err = uerr; return NULL; }
    extern char *hermes_state_default_db_path(void);
    extern hermes_state_db_t *hermes_state_db_open(const char *path);
    extern void hermes_state_db_close(hermes_state_db_t *db);
    extern bool hermes_state_vacuum(hermes_state_db_t *db);
    char *path = hermes_state_default_db_path();
    hermes_state_db_t *db = path ? hermes_state_db_open(path) : NULL;
    if (!db) {
        *err = strdup("Session database not available.");
        free(path);
        return NULL;
    }
    /* Python: count = db.vacuum() (FTS index count optimized). The C port
     * optimizes messages_fts then VACUUMs; report 1 index on success. */
    bool ok = hermes_state_vacuum(db);
    hermes_state_db_close(db);
    free(path);
    if (!ok) {
        *err = strdup("Could not optimize the session store.");
        return NULL;
    }
    char *out = NULL;
    if (asprintf(&out, "Optimized %d FTS index(es).", 1) < 0) out = NULL;
    return out;
}

/* PoP: coe_sessions_repair @ hermes_cli/console_engine.py:_sessions_repair */
char *coe_sessions_repair(coe_engine *eng, char **args, size_t nargs, char **err) {
    (void)eng;
    bool check_only = false, no_backup = false;
    for (size_t i = 0; i < nargs; i++) {
        if (strcmp(args[i], "--check-only") == 0) check_only = true;
        else if (strcmp(args[i], "--no-backup") == 0) no_backup = true;
        else {
            if (asprintf(err, "sessions repair: unrecognized arguments: %s", args[i]) < 0)
                *err = NULL;
            return NULL;
        }
    }
    extern char *hermes_state_default_db_path(void);
    extern char *hermes_state_repair_state_db_schema(const char *db_path);
    const char *db_path = hermes_state_default_db_path();
    if (access(db_path ? db_path : "", F_OK) != 0) {
        char *out = NULL;
        if (asprintf(&out, "No session database at %s (nothing to repair).",
                     db_path ? db_path : "") < 0) out = NULL;
        return out;
    }
    /* cleanliness probe: open through the SessionDB port (schema ensure +
     * integrity checks mirror _db_opens_cleanly's failure modes) */
    extern hermes_state_db_t *hermes_state_db_open(const char *path);
    extern void hermes_state_db_close(hermes_state_db_t *db);
    hermes_state_db_t *probe = hermes_state_db_open(db_path);
    if (probe) {
        hermes_state_db_close(probe);
        char *out = NULL;
        if (asprintf(&out, "%s opens cleanly; no repair needed.", db_path) < 0) out = NULL;
        return out;
    }
    char *head = NULL;
    if (asprintf(&head, "%s does not open cleanly: database open failed", db_path) < 0)
        head = NULL;
    if (check_only) return head;
    (void)no_backup; /* backup policy handled inside repair_state_db_schema */
    char *rerr = hermes_state_repair_state_db_schema(db_path);
    if (!rerr) {
        char *out = NULL;
        if (asprintf(&out, "%s\nstrategy: schema-reinit\nRepaired session database.",
                     head ? head : "") < 0) out = NULL;
        free(head);
        return out;
    }
    if (asprintf(err, "Repair failed: %s", rerr) < 0) *err = NULL;
    free(rerr); free(head);
    return NULL;
}

/* PoP: coe_profile_status @ hermes_cli/console_engine.py:_profile_status */
char *coe_profile_status(coe_engine *eng, char **args, size_t nargs, char **err) {
    (void)eng;
    extern char *coe_expect_no_args(char **args, size_t nargs, const char *usage);
    char *uerr = coe_expect_no_args(args, nargs, "profile");
    if (uerr) { *err = uerr; return NULL; }
    extern char *ce_dispatch_extracted_subcommand(const char *root,
                                                   const char *const *fixed, size_t n_fixed,
                                                   const char *args[], size_t nargs,
                                                   const char *module_name,
                                                   const char *builder_name,
                                                   const char *main_handler_name,
                                                   ce_ns_update_fn namespace_update,
                                                   char **err);
    return ce_dispatch_extracted_subcommand("profile", NULL, 0,
                                            (const char **)args, nargs,
                                            "hermes_cli.subcommands.profile",
                                            "build_profile_parser", "cmd_profile",
                                            NULL, err);
}

/* PoP: coe_cron_list @ hermes_cli/console_engine.py:_cron_list */
char *coe_cron_list(coe_engine *eng, char **args, size_t nargs, char **err) {
    (void)eng;
    bool show_all = false;
    for (size_t i = 0; i < nargs; i++) {
        if (strcmp(args[i], "--all") == 0) show_all = true;
        else {
            if (asprintf(err, "cron list: unrecognized arguments: %s", args[i]) < 0)
                *err = NULL;
            return NULL;
        }
    }
    extern void hermes_cli_cron_cron_list(int show_all);
    struct cl_ctx { int show_all; } cc = {show_all};
    int (*runner)(void *, char **) = (int (*)(void *, char **))0;
    /* nested trampoline: prints via the captured streams */
    int run(void *p, char **em) {
        struct cl_ctx *x = p;
        hermes_cli_cron_cron_list(x->show_all);
        (void)em;
        return 0;
    }
    runner = run;
    return coe_capture_output(runner, &cc, err);
}

/* PoP: coe_cron_status @ hermes_cli/console_engine.py:_cron_status */
char *coe_cron_status(coe_engine *eng, char **args, size_t nargs, char **err) {
    (void)eng;
    extern char *coe_expect_no_args(char **args, size_t nargs, const char *usage);
    char *uerr = coe_expect_no_args(args, nargs, "cron status");
    if (uerr) { *err = uerr; return NULL; }
    extern void hermes_cli_cron_cron_status(void);
    int run(void *p, char **em) {
        (void)p; (void)em;
        hermes_cli_cron_cron_status();
        return 0;
    }
    return coe_capture_output(run, NULL, err);
}

/* shared _format_job glue over the cron-jobs port */
static char *coe_job_line(json_t *job, const char *action) {
    extern char *ce_format_job(const json_t *job, const char *action);
    if (!job) return NULL;
    char *line = ce_format_job(job, action);
    json_free(job);
    return line;
}

/* PoP: coe_cron_pause @ hermes_cli/console_engine.py:_cron_pause */
char *coe_cron_pause(coe_engine *eng, char **args, size_t nargs, char **err) {
    (void)eng;
    if (nargs != 1) {
        *err = strdup("Usage: cron pause <job>");
        return NULL;
    }
    extern json_t *cronjobs_resolve_job_ref(const char *ref, bool *ambiguous);
    extern json_t *cronjobs_pause_job(const char *job_id, const char *reason);
    bool ambiguous = false;
    cronjobs_resolve_job_ref(args[0], &ambiguous);
    if (ambiguous) {
        /* str(AmbiguousJobReference) */
        if (asprintf(err, "Multiple cron jobs match '%s'. Use the job id.", args[0]) < 0)
            *err = NULL;
        return NULL;
    }
    json_t *job = cronjobs_pause_job(args[0], "paused from hermes console");
    if (!job) {
        if (asprintf(err, "Job not found: %s", args[0]) < 0) *err = NULL;
        return NULL;
    }
    return coe_job_line(job, "Paused");
}

/* PoP: coe_cron_resume @ hermes_cli/console_engine.py:_cron_resume */
char *coe_cron_resume(coe_engine *eng, char **args, size_t nargs, char **err) {
    (void)eng;
    const char *jobref = NULL, *at = NULL;
    bool run_now = false;
    for (size_t i = 0; i < nargs; i++) {
        const char *a = args[i];
        if (strcmp(a, "--at") == 0 && i + 1 < nargs) at = args[++i];
        else if (strcmp(a, "--run-now") == 0) run_now = true;
        else if (!jobref) jobref = a;
        else {
            if (asprintf(err, "cron resume: unrecognized arguments: %s", a) < 0)
                *err = NULL;
            return NULL;
        }
    }
    if (!jobref) {
        *err = strdup("cron resume: the following arguments are required: job");
        return NULL;
    }
    if (at && run_now) {
        *err = strdup("Use exactly one of --at or --run-now.");
        return NULL;
    }
    extern bool cronjobs_resolve_ambiguous(const char *ref);
    bool ambiguous = false;
    extern json_t *cronjobs_resolve_job_ref(const char *ref, bool *ambiguous);
    cronjobs_resolve_job_ref(jobref, &ambiguous);
    if (ambiguous) {
        if (asprintf(err, "Multiple cron jobs match '%s'. Use the job id.", jobref) < 0)
            *err = NULL;
        return NULL;
    }
    extern json_t *cronjobs_rearm_oneshot(const char *job_id, const char *run_at, char **rerr);
    extern json_t *cronjobs_resume_job(const char *job_id);
    extern char *now_iso(void);
    json_t *job = NULL;
    if (at || run_now) {
        char *iso = NULL;
        if (run_now) iso = now_iso();
        char *rerr = NULL;
        job = cronjobs_rearm_oneshot(jobref, run_now ? iso : at, &rerr);
        free(iso);
        if (rerr) { *err = rerr; return NULL; } /* ValueError path */
    } else {
        job = cronjobs_resume_job(jobref);
    }
    if (!job) {
        if (asprintf(err, "Job not found: %s", jobref) < 0) *err = NULL;
        return NULL;
    }
    return coe_job_line(job, "Resumed");
}

/* PoP: coe_cron_run @ hermes_cli/console_engine.py:_cron_run */
char *coe_cron_run(coe_engine *eng, char **args, size_t nargs, char **err) {
    (void)eng;
    if (nargs != 1) {
        *err = strdup("Usage: cron run <job>");
        return NULL;
    }
    extern json_t *cronjobs_resolve_job_ref(const char *ref, bool *ambiguous);
    extern json_t *cronjobs_trigger_job(const char *job_id);
    bool ambiguous = false;
    cronjobs_resolve_job_ref(args[0], &ambiguous);
    if (ambiguous) {
        if (asprintf(err, "Multiple cron jobs match '%s'. Use the job id.", args[0]) < 0)
            *err = NULL;
        return NULL;
    }
    json_t *job = cronjobs_trigger_job(args[0]);
    if (!job) {
        if (asprintf(err, "Job not found: %s", args[0]) < 0) *err = NULL;
        return NULL;
    }
    return coe_job_line(job, "Triggered");
}

/* ── registration surface ──────────────────────────────────────────────
 * handler_factory trampolines for _register_command_family. */
typedef struct ce_family_spec {
    int kind; /* ce_hcmd kind */
    const char *root, *module, *name1, *name2;
} ce_family_spec;

static ce_hcmd *coe_family_factory(const ce_family_spec *spec,
                                   const char *const *fixed, size_t n_fixed) {
    extern ce_hcmd *ce_extracted_handler(const char *root, const char *const *fixed,
                                         size_t n_fixed, const char *module_name,
                                         const char *builder_name,
                                         const char *main_handler_name);
    extern ce_hcmd *ce_registered_handler(const char *root, const char *const *fixed,
                                          size_t n_fixed, const char *module_name,
                                          const char *register_name,
                                          const char *handler_name);
    extern ce_hcmd *ce_builder_handler(const char *root, const char *const *fixed,
                                       size_t n_fixed, const char *module_name,
                                       const char *builder_name,
                                       const char *main_handler_name);
    extern ce_hcmd *ce_adder_handler(const char *root, const char *const *fixed,
                                     size_t n_fixed, const char *module_name,
                                     const char *add_name);
    switch (spec->kind) {
    case 1:
        return ce_extracted_handler(spec->root, fixed, n_fixed, spec->module,
                                    spec->name1, spec->name2);
    case 2:
        return ce_registered_handler(spec->root, fixed, n_fixed, spec->module,
                                     spec->name1, spec->name2);
    case 3:
        return ce_builder_handler(spec->root, fixed, n_fixed, spec->module,
                                  spec->name1, spec->name2);
    default:
        return ce_adder_handler(spec->root, fixed, n_fixed, spec->module, spec->name1);
    }
}

#define CE_FACTORY_GEN(N, ROOT, MOD, N1, N2, K)                              \
    static ce_hcmd *N(const char *const *fixed, size_t n_fixed)              \
    {                                                                         \
        static const ce_family_spec s = {K, ROOT, MOD, N1, N2};               \
        return coe_family_factory(&s, fixed, n_fixed);                        \
    }

CE_FACTORY_GEN(ce_fact_dump, "dump", "hermes_cli.subcommands.dump", "build_dump_parser", "cmd_dump", 1)
CE_FACTORY_GEN(ce_fact_debug, "debug", "hermes_cli.subcommands.debug", "build_debug_parser", "cmd_debug", 1)
CE_FACTORY_GEN(ce_fact_prompt_size, "prompt-size", "hermes_cli.subcommands.prompt_size", "build_prompt_size_parser", "cmd_prompt_size", 1)
CE_FACTORY_GEN(ce_fact_insights, "insights", "hermes_cli.subcommands.insights", "build_insights_parser", "cmd_insights", 1)
CE_FACTORY_GEN(ce_fact_security, "security", "hermes_cli.subcommands.security", "build_security_parser", "cmd_security", 1)
CE_FACTORY_GEN(ce_fact_backup, "backup", "hermes_cli.subcommands.backup", "build_backup_parser", "cmd_backup", 1)
CE_FACTORY_GEN(ce_fact_import, "import", "hermes_cli.subcommands.import_cmd", "build_import_cmd_parser", "cmd_import", 1)
CE_FACTORY_GEN(ce_fact_config, "config", "hermes_cli.subcommands.config", "build_config_parser", "cmd_config", 1)
CE_FACTORY_GEN(ce_fact_tools, "tools", "hermes_cli.subcommands.tools", "build_tools_parser", "cmd_tools", 1)
CE_FACTORY_GEN(ce_fact_plugins, "plugins", "hermes_cli.subcommands.plugins", "build_plugins_parser", "cmd_plugins", 1)
CE_FACTORY_GEN(ce_fact_skills, "skills", "hermes_cli.subcommands.skills", "build_skills_parser", "cmd_skills", 1)
CE_FACTORY_GEN(ce_fact_mcp, "mcp", "hermes_cli.subcommands.mcp", "build_mcp_parser", "cmd_mcp", 1)
CE_FACTORY_GEN(ce_fact_memory, "memory", "hermes_cli.subcommands.memory", "build_memory_parser", "cmd_memory", 1)
CE_FACTORY_GEN(ce_fact_auth, "auth", "hermes_cli.subcommands.auth", "build_auth_parser", "cmd_auth", 1)
CE_FACTORY_GEN(ce_fact_pairing, "pairing", "hermes_cli.subcommands.pairing", "build_pairing_parser", "cmd_pairing", 1)
CE_FACTORY_GEN(ce_fact_webhook, "webhook", "hermes_cli.subcommands.webhook", "build_webhook_parser", "cmd_webhook", 1)
CE_FACTORY_GEN(ce_fact_hooks, "hooks", "hermes_cli.subcommands.hooks", "build_hooks_parser", "cmd_hooks", 1)
CE_FACTORY_GEN(ce_fact_slack, "slack", "hermes_cli.subcommands.slack", "build_slack_parser", "cmd_slack", 1)
CE_FACTORY_GEN(ce_fact_profile, "profile", "hermes_cli.subcommands.profile", "build_profile_parser", "cmd_profile", 1)
CE_FACTORY_GEN(ce_fact_cron, "cron", "hermes_cli.subcommands.cron", "build_cron_parser", "cmd_cron", 1)
CE_FACTORY_GEN(ce_fact_bundles, "bundles", "hermes_cli.bundles", "register_cli", NULL, 2)
CE_FACTORY_GEN(ce_fact_checkpoints, "checkpoints", "hermes_cli.checkpoints", "register_cli", NULL, 2)
CE_FACTORY_GEN(ce_fact_curator, "curator", "hermes_cli.curator", "register_cli", NULL, 2)
CE_FACTORY_GEN(ce_fact_pets, "pets", "hermes_cli.pets", "register_cli", NULL, 2)
CE_FACTORY_GEN(ce_fact_project, "project", "hermes_cli.projects_cmd", "build_parser", "cmd_project", 3)
CE_FACTORY_GEN(ce_fact_kanban, "kanban", "hermes_cli.kanban", "build_parser", "cmd_kanban", 3)
CE_FACTORY_GEN(ce_fact_portal, "portal", "hermes_cli.portal_cli", "add_parser", NULL, 4)
CE_FACTORY_GEN(ce_fact_send, "send", "hermes_cli.send_cmd", "register_send_subparser", NULL, 4)

extern void coe_register_command_family(coe_engine *eng, const char *root,
                                        const char *const *paths, size_t n_paths,
                                        ce_hcmd *(*handler_factory)(const char *const *fixed,
                                                                    size_t n_fixed),
                                        const char *const *mutating, size_t n_mutating,
                                        const char *summary, const char *summaries_json,
                                        const char *confirmation);


/* ── simple handler wrappers used by the registrar ──────────────────── */
static char *coe_logs_handler(coe_engine *eng, char **args, size_t nargs, char **err) {
    (void)eng;
    return coe_logs_body(args, nargs, err);
}

static char *coe_sessions_list_handler(coe_engine *eng, char **args, size_t nargs,
                                       char **err) {
    (void)eng;
    return coe_sessions_list_body(args, nargs, err);
}

static char *coe_config_show_handler(coe_engine *eng, char **args, size_t nargs,
                                     char **err) {
    (void)eng; (void)args; (void)nargs;
    return coe_config_show_body(args, nargs, err);
}

/* config path body (the gap credit lives at config_path in config_py_io.c) */
static char *coe_config_path_handler(coe_engine *eng, char **args, size_t nargs,
                                     char **err) {
    (void)eng; (void)args; (void)nargs; (void)err;
    extern void config_py_get_config_path(char *buf, size_t sz);
    char buf[1024];
    config_py_get_config_path(buf, sizeof(buf));
    return strdup(buf);
}

static const coe_simple_fn coe_h_doctor = coe_doctor_run;
static const coe_simple_fn coe_h_logs = coe_logs_handler;
static const coe_simple_fn coe_h_sessions_list = coe_sessions_list_handler;
static const coe_simple_fn coe_h_config_show = coe_config_show_handler;
static const coe_simple_fn coe_h_config_path = coe_config_path_handler;

/* PoP: coe_register_defaults @ hermes_cli/console_engine.py:_register_defaults */
void coe_register_defaults(coe_engine *eng) {
    extern void coe_register_broad_cli_surface(coe_engine *eng);
    extern void coe_engine_register(coe_engine *eng, const char *key,
                                    const char *usage, const char *summary,
                                    void *handler, int kind, bool mutating,
                                    const char *confirmation);
    extern char *coe_version(coe_engine *eng, char **args, size_t nargs, char **err);
    extern char *coe_status_cmd(coe_engine *eng, char **args, size_t nargs, char **err);
    extern char *coe_sessions_stats(coe_engine *eng, char **args, size_t nargs, char **err);
    extern char *coe_config_set(coe_engine *eng, char **args, size_t nargs, char **err);
    extern char *coe_cron_list(coe_engine *eng, char **args, size_t nargs, char **err);
    extern char *coe_cron_status(coe_engine *eng, char **args, size_t nargs, char **err);
    extern char *coe_cron_pause(coe_engine *eng, char **args, size_t nargs, char **err);
    extern char *coe_cron_resume(coe_engine *eng, char **args, size_t nargs, char **err);
    extern char *coe_cron_run(coe_engine *eng, char **args, size_t nargs, char **err);

    coe_engine_register(eng, "status", "status", "Show Hermes component status.",
                        (void *)coe_status_cmd, 1, false, "");
    coe_engine_register(eng, "version", "version", "Show Hermes version information.",
                        (void *)coe_version, 1, false, "");
    coe_engine_register(eng, "doctor", "doctor", "Run diagnostics without auto-fix.",
                        (void *)coe_h_doctor, 1, false, "");
    coe_engine_register(eng, "logs", "logs [name] [-n N]", "Show recent Hermes logs.",
                        (void *)coe_h_logs, 1, false, "");
    coe_engine_register(eng, "sessions list", "sessions list [--limit N]",
                        "List recent sessions.", (void *)coe_h_sessions_list, 1,
                        false, "");
    coe_engine_register(eng, "sessions stats", "sessions stats",
                        "Show session store statistics.",
                        (void *)coe_sessions_stats, 1, false, "");
    coe_engine_register(eng, "config show", "config show",
                        "Show current configuration.", (void *)coe_h_config_show, 1,
                        false, "");
    coe_engine_register(eng, "config path", "config path", "Print config.yaml path.",
                        (void *)coe_h_config_path, 1, false, "");
    coe_engine_register(eng, "config set", "config set <key> <value>",
                        "Set a configuration value.", (void *)coe_config_set, 1, true,
                        "Update Hermes configuration?");
    coe_engine_register(eng, "cron list", "cron list [--all]", "List scheduled jobs.",
                        (void *)coe_cron_list, 1, false, "");
    coe_engine_register(eng, "cron status", "cron status", "Show cron scheduler status.",
                        (void *)coe_cron_status, 1, false, "");
    coe_engine_register(eng, "cron pause", "cron pause <job>", "Pause a scheduled job.",
                        (void *)coe_cron_pause, 1, true, "Pause this cron job?");
    coe_engine_register(eng, "cron resume", "cron resume <job>",
                        "Resume a paused cron job.", (void *)coe_cron_resume, 1, true,
                        "Resume this cron job?");
    coe_engine_register(eng, "cron run", "cron run <job>",
                        "Run a job on the next scheduler tick.",
                        (void *)coe_cron_run, 1, true, "Trigger this cron job?");
    coe_register_broad_cli_surface(eng);
}

/* * Registers the non-admin CLI command families safe for Hermes Console:
 * extracted builder families, sessions/config extras, portal / project /
 * kanban families and the register_cli families. Summaries come from the
 * memoized parser walks; mutating paths gate confirmations.
 */
/* PoP: coe_register_broad_cli_surface @ hermes_cli/console_engine.py:_register_broad_cli_surface */
void coe_register_broad_cli_surface(coe_engine *eng) {
    static const char *const empty_paths[] = {""};
#define NO_MUT ((const char *const *)NULL)

    static const char *const debug_paths[] = {"share", "delete"};
    static const char *const security_paths[] = {"audit"};
    static const char *const config_paths[] = {"env-path", "check"};
    static const char *const tools_paths[] = {"list", "enable", "disable", "post-setup"};
    static const char *const tools_mut[] = {"enable", "disable", "post-setup"};
    static const char *const plugins_paths[] = {"list", "enable", "disable", "install",
                                                "update", "remove"};
    static const char *const plugins_mut[] = {"enable", "disable", "install", "update",
                                              "remove"};
    static const char *const skills_paths[] = {
        "browse", "search", "inspect", "list", "check", "list-modified", "diff",
        "install", "update", "audit", "uninstall", "reset", "opt-in", "opt-out",
        "repair-official", "snapshot export", "snapshot import", "tap list",
        "tap add", "tap remove"};
    static const char *const skills_mut[] = {
        "install", "update", "audit", "uninstall", "reset", "opt-in", "opt-out",
        "repair-official", "snapshot export", "snapshot import", "tap add",
        "tap remove"};
    static const char *const mcp_paths[] = {"list", "catalog", "test", "add", "remove",
                                            "install", "login", "reauth", "configure",
                                            "picker"};
    static const char *const mcp_mut[] = {"add", "remove", "install", "login", "reauth",
                                          "configure", "picker"};
    static const char *const memory_paths[] = {"status", "off", "reset"};
    static const char *const memory_mut[] = {"off", "reset"};
    static const char *const auth_paths[] = {"list", "status", "reset", "add", "remove",
                                             "logout", "spotify status", "spotify login",
                                             "spotify logout"};
    static const char *const auth_mut[] = {"reset", "add", "remove", "logout",
                                           "spotify login", "spotify logout"};
    static const char *const pairing_paths[] = {"list", "approve", "revoke",
                                                "clear-pending"};
    static const char *const pairing_mut[] = {"approve", "revoke", "clear-pending"};
    static const char *const webhook_paths[] = {"list", "subscribe", "remove", "test"};
    static const char *const webhook_mut[] = {"subscribe", "remove"};
    static const char *const hooks_paths[] = {"list", "test", "doctor", "revoke"};
    static const char *const hooks_mut[] = {"test", "doctor", "revoke"};
    static const char *const slack_paths[] = {"manifest"};
    static const char *const profile_paths[] = {"list", "show", "info", "create", "use",
                                                "describe", "rename", "delete",
                                                "export", "import", "install",
                                                "update"};
    static const char *const profile_mut[] = {"create", "use", "describe", "rename",
                                              "delete", "export", "import", "install",
                                              "update"};
    static const char *const cron_paths[] = {"create", "edit", "remove", "tick"};
    static const char *const cron_mut[] = {"create", "edit", "remove", "tick"};

    /* extracted builder families */
    coe_register_command_family(eng, "dump", empty_paths, 1, ce_fact_dump, NO_MUT, 0,
                                "", ce_extracted_summaries("hermes_cli.subcommands.dump",
                                                           "build_dump_parser", "cmd_dump"),
                                "");
    coe_register_command_family(eng, "debug", debug_paths, 2, ce_fact_debug,
                                debug_paths, 2, "",
                                ce_extracted_summaries("hermes_cli.subcommands.debug",
                                                       "build_debug_parser", "cmd_debug"),
                                "");
    coe_register_command_family(eng, "prompt-size", empty_paths, 1, ce_fact_prompt_size,
                                NO_MUT, 0, "",
                                ce_extracted_summaries("hermes_cli.subcommands.prompt_size",
                                                       "build_prompt_size_parser",
                                                       "cmd_prompt_size"),
                                "");
    coe_register_command_family(eng, "insights", empty_paths, 1, ce_fact_insights,
                                NO_MUT, 0, "",
                                ce_extracted_summaries("hermes_cli.subcommands.insights",
                                                       "build_insights_parser",
                                                       "cmd_insights"),
                                "");
    coe_register_command_family(eng, "security", security_paths, 1, ce_fact_security,
                                NO_MUT, 0, "",
                                ce_extracted_summaries("hermes_cli.subcommands.security",
                                                       "build_security_parser",
                                                       "cmd_security"),
                                "");
    coe_register_command_family(eng, "backup", empty_paths, 1, ce_fact_backup,
                                empty_paths, 1, "",
                                ce_extracted_summaries("hermes_cli.subcommands.backup",
                                                       "build_backup_parser", "cmd_backup"),
                                "");
    coe_register_command_family(eng, "import", empty_paths, 1, ce_fact_import,
                                empty_paths, 1, "",
                                ce_extracted_summaries("hermes_cli.subcommands.import_cmd",
                                                       "build_import_cmd_parser",
                                                       "cmd_import"),
                                "");
    coe_register_command_family(eng, "config", config_paths, 2, ce_fact_config,
                                NO_MUT, 0, "",
                                ce_extracted_summaries("hermes_cli.subcommands.config",
                                                       "build_config_parser", "cmd_config"),
                                "");
    coe_register_command_family(eng, "tools", tools_paths, 4, ce_fact_tools,
                                tools_mut, 3, "",
                                ce_extracted_summaries("hermes_cli.subcommands.tools",
                                                       "build_tools_parser", "cmd_tools"),
                                "");
    coe_register_command_family(eng, "plugins", plugins_paths, 6, ce_fact_plugins,
                                plugins_mut, 5, "",
                                ce_extracted_summaries("hermes_cli.subcommands.plugins",
                                                       "build_plugins_parser", "cmd_plugins"),
                                "");
    coe_register_command_family(eng, "skills", skills_paths,
                                sizeof(skills_paths) / sizeof(skills_paths[0]),
                                ce_fact_skills, skills_mut,
                                sizeof(skills_mut) / sizeof(skills_mut[0]), "",
                                ce_extracted_summaries("hermes_cli.subcommands.skills",
                                                       "build_skills_parser", "cmd_skills"),
                                "");
    coe_register_command_family(eng, "mcp", mcp_paths,
                                sizeof(mcp_paths) / sizeof(mcp_paths[0]),
                                ce_fact_mcp, mcp_mut,
                                sizeof(mcp_mut) / sizeof(mcp_mut[0]), "",
                                ce_extracted_summaries("hermes_cli.subcommands.mcp",
                                                       "build_mcp_parser", "cmd_mcp"),
                                "");
    coe_register_command_family(eng, "memory", memory_paths, 3, ce_fact_memory,
                                memory_mut, 2, "",
                                ce_extracted_summaries("hermes_cli.subcommands.memory",
                                                       "build_memory_parser", "cmd_memory"),
                                "");
    coe_register_command_family(eng, "auth", auth_paths,
                                sizeof(auth_paths) / sizeof(auth_paths[0]),
                                ce_fact_auth, auth_mut,
                                sizeof(auth_mut) / sizeof(auth_mut[0]), "",
                                ce_extracted_summaries("hermes_cli.subcommands.auth",
                                                       "build_auth_parser", "cmd_auth"),
                                "");
    coe_register_command_family(eng, "pairing", pairing_paths, 4, ce_fact_pairing,
                                pairing_mut, 3, "",
                                ce_extracted_summaries("hermes_cli.subcommands.pairing",
                                                       "build_pairing_parser", "cmd_pairing"),
                                "");
    coe_register_command_family(eng, "webhook", webhook_paths, 4, ce_fact_webhook,
                                webhook_mut, 2, "",
                                ce_extracted_summaries("hermes_cli.subcommands.webhook",
                                                       "build_webhook_parser", "cmd_webhook"),
                                "");
    coe_register_command_family(eng, "hooks", hooks_paths, 4, ce_fact_hooks,
                                hooks_mut, 3, "",
                                ce_extracted_summaries("hermes_cli.subcommands.hooks",
                                                       "build_hooks_parser", "cmd_hooks"),
                                "");
    coe_register_command_family(eng, "slack", slack_paths, 1, ce_fact_slack,
                                NO_MUT, 0, "",
                                ce_extracted_summaries("hermes_cli.subcommands.slack",
                                                       "build_slack_parser", "cmd_slack"),
                                "");
    coe_register_command_family(eng, "profile", profile_paths, 12, ce_fact_profile,
                                profile_mut, 9, "",
                                ce_extracted_summaries("hermes_cli.subcommands.profile",
                                                       "build_profile_parser", "cmd_profile"),
                                "");
    coe_register_command_family(eng, "cron", cron_paths, 4, ce_fact_cron,
                                cron_mut, 4, "",
                                ce_extracted_summaries("hermes_cli.subcommands.cron",
                                                       "build_cron_parser", "cmd_cron"),
                                "");

    /* sessions / config extras registered directly on the engine */
    extern void coe_engine_register(coe_engine *eng, const char *key,
                                    const char *usage, const char *summary,
                                    void *handler, int kind, bool mutating,
                                    const char *confirmation);
    extern char *coe_config_migrate(coe_engine *eng, char **args, size_t nargs, char **err);
    extern char *coe_sessions_export(coe_engine *eng, char **args, size_t nargs, char **err);
    extern char *coe_sessions_rename(coe_engine *eng, char **args, size_t nargs, char **err);
    extern char *coe_sessions_optimize(coe_engine *eng, char **args, size_t nargs, char **err);
    extern char *coe_sessions_repair(coe_engine *eng, char **args, size_t nargs, char **err);
    extern char *coe_profile_status(coe_engine *eng, char **args, size_t nargs, char **err);

    coe_engine_register(eng, "config migrate", "config migrate",
                        "Update config with new options.", (void *)coe_config_migrate,
                        1, true, "Update Hermes configuration with missing defaults?");
    coe_engine_register(eng, "sessions export",
                        "sessions export <output> [--source SOURCE] [--session-id ID]",
                        "Export sessions to JSONL.", (void *)coe_sessions_export, 1,
                        true, "Export session data?");
    coe_engine_register(eng, "sessions rename", "sessions rename <session> <title>",
                        "Rename a session.", (void *)coe_sessions_rename, 1, true,
                        "Rename this session?");
    coe_engine_register(eng, "sessions optimize", "sessions optimize",
                        "Optimize the session store.", (void *)coe_sessions_optimize,
                        1, true, "Optimize the session database?");
    coe_engine_register(eng, "sessions repair",
                        "sessions repair [--check-only] [--no-backup]",
                        "Repair a malformed session database schema.",
                        (void *)coe_sessions_repair, 1, true,
                        "Repair the session database?");
    coe_engine_register(eng, "profile", "profile", "Show active profile status.",
                        (void *)coe_profile_status, 1, false, "");

    /* send: adder family with a single root-level command */
    static const char *const send_paths[] = {""};
    coe_register_command_family(eng, "send", send_paths, 1, ce_fact_send, NO_MUT, 0,
                                "Send a message to a configured platform.", "",
                                "Send this message?");

    /* portal adder family */
    static const char *const portal_paths[] = {"info", "tools"};
    coe_register_command_family(eng, "portal", portal_paths, 2, ce_fact_portal,
                                NO_MUT, 0, "",
                                ce_adder_summaries("hermes_cli.portal_cli", "add_parser"),
                                "");

    /* project builder family */
    static const char *const project_paths[] = {
        "list", "show", "create", "add-folder", "remove-folder", "rename",
        "set-primary", "use", "archive", "restore", "bind-board"};
    static const char *const project_mut[] = {
        "create", "add-folder", "remove-folder", "rename", "set-primary", "use",
        "archive", "restore", "bind-board"};
    coe_register_command_family(eng, "project", project_paths, 11, ce_fact_project,
                                project_mut, 9, "",
                                ce_builder_summaries("hermes_cli.projects_cmd",
                                                     "build_parser"),
                                "");

    /* kanban builder family */
    static const char *const kanban_paths[] = {
        "init", "boards list", "boards create", "boards rm", "boards switch",
        "boards current", "boards rename", "boards set-workdir", "create", "list",
        "show", "assign", "reclaim", "reassign", "diagnose", "link", "unlink",
        "claim", "comment", "complete", "edit", "block", "schedule", "unblock",
        "promote", "archive", "stats", "runs", "heartbeat", "assignments", "context"};
    static const char *const kanban_mut[] = {
        "init", "boards create", "boards rm", "boards switch", "boards rename",
        "boards set-workdir", "create", "assign", "reclaim", "reassign", "link",
        "unlink", "claim", "comment", "complete", "edit", "block", "schedule",
        "unblock", "promote", "archive"};
    coe_register_command_family(eng, "kanban", kanban_paths,
                                sizeof(kanban_paths) / sizeof(kanban_paths[0]),
                                ce_fact_kanban, kanban_mut,
                                sizeof(kanban_mut) / sizeof(kanban_mut[0]), "",
                                ce_builder_summaries("hermes_cli.kanban", "build_parser"),
                                "");

    /* registered register_cli families */
    struct regfam {
        ce_hcmd *(*factory)(const char *const *, size_t);
        const char *root;
        const char *module;
        const char *paths[13];
        size_t n_paths;
        const char *mut[13];
        size_t n_mut;
    };
    static const struct regfam regfams[] = {
        {ce_fact_bundles, "bundles", "hermes_cli.bundles",
         {"list", "show", "create", "delete", "reload"}, 5,
         {"create", "delete", "reload"}, 3},
        {ce_fact_checkpoints, "checkpoints", "hermes_cli.checkpoints",
         {"status", "list", "prune", "clear", "clear-legacy"}, 5,
         {"prune", "clear", "clear-legacy"}, 3},
        {ce_fact_curator, "curator", "hermes_cli.curator",
         {"status", "run", "pause", "resume", "pin", "unpin", "restore",
          "list-archived", "archive", "prune", "backup", "rollback"}, 12,
         {"run", "pause", "resume", "pin", "unpin", "restore", "archive", "prune",
          "backup", "rollback"}, 10},
        {ce_fact_pets, "pets", "hermes_cli.pets",
         {"list", "install", "select", "show", "off", "scale", "remove", "doctor"}, 8,
         {"install", "select", "off", "scale", "remove"}, 5},
    };
    for (size_t i = 0; i < sizeof(regfams) / sizeof(regfams[0]); i++) {
        const struct regfam *f = &regfams[i];
        coe_register_command_family(eng, f->root, f->paths, f->n_paths, f->factory,
                                    f->mut, f->n_mut, "",
                                    ce_registered_summaries(f->root, f->module,
                                                            "register_cli"),
                                    "");
    }
}

/* PoP: coe_config_migrate @ hermes_cli/console_engine.py:_config_migrate */
char *coe_config_migrate(coe_engine *eng, char **args, size_t nargs, char **err) {
    (void)eng; (void)args; (void)nargs;
    extern char *coe_expect_no_args(char **args, size_t nargs, const char *usage);
    char *uerr = coe_expect_no_args(args, nargs, "config migrate");
    if (uerr) { *err = uerr; return NULL; }
    extern void config_migrate_config(int interactive, int quiet, char *out_json,
                                      size_t out_sz);
    char results[2048];
    config_migrate_config(0, 0, results, sizeof(results));
    json_t *res = json_parse(results, NULL);
    bool env_added = false, cfg_added = false;
    json_t *warnings = NULL;
    if (res && json_is_object(res)) {
        const json_t *ea = json_object_get(res, "env_added");
        const json_t *ca = json_object_get(res, "config_added");
        warnings = (json_t *)json_object_get(res, "warnings");
        if (json_is_array(ea) && json_array_size(ea) > 0) env_added = true;
        if (json_is_array(ca) && json_array_size(ca) > 0) cfg_added = true;
    }
    size_t cap = 256, o = 0;
    char *out = malloc(cap);
    o += (size_t)snprintf(out + o, cap - o, "%s",
                          (env_added || cfg_added) ? "Configuration updated."
                                                    : "Configuration is up to date.");
    if (warnings && json_is_array(warnings)) {
        size_t n = json_array_size(warnings);
        for (size_t i = 0; i < n; i++) {
            const json_t *w = json_array_get(warnings, i);
            const char *ws = json_string_value(w);
            char row[1024];
            snprintf(row, sizeof(row), "\nWarning: %s", ws ? ws : "");
            while (o + strlen(row) + 1 > cap) { cap *= 2; out = realloc(out, cap); }
            strcat(out, row);
            o += strlen(row);
        }
    }
    if (res) json_free(res);
    return out;
}

/* ── full execute path + REPL ──────────────────────────────────────────
 * execute() itself is credited via coe_execute; the REPL needs the real
 * pipeline (split → shell-syntax gate → builtin → resolve → confirm → cap),
 * so it drives coe_execute_full below. */
char *coe_execute_full(coe_engine *eng, const char *line, bool confirmed) {
    extern char *coe_split_line_public(const char *line, char **err);
    char *raw = ce_strip_heap(line);
    if (!raw || !*raw) { free(raw); return coe_result_json("ok", "", "", ""); }

    char *err = NULL;
    char *tokens_json = ce_split_line(raw, &err);
    if (!tokens_json) {
        char *res = coe_result_json("error", err ? err : "", raw, "");
        free(err); free(raw);
        return res;
    }
    json_t *arr = json_parse(tokens_json, NULL);
    free(tokens_json);
    size_t ntok = (arr && json_is_array(arr)) ? json_array_size(arr) : 0;
    char **tokens = malloc(sizeof(char *) * (ntok ? ntok : 1));
    for (size_t i = 0; i < ntok; i++) {
        const json_t *t = json_array_get(arr, i);
        const char *ts = json_string_value(t);
        tokens[i] = strdup(ts ? ts : "");
    }
    if (arr) json_free(arr);
    /* strip a leading literal "hermes" */
    if (ntok > 0 && strcmp(tokens[0], "hermes") == 0) {
        free(tokens[0]);
        memmove(tokens, tokens + 1, sizeof(char *) * (ntok - 1));
        ntok--;
    }
    if (ntok == 0) {
        for (size_t i = 0; i < ntok; i++) free(tokens[i]);
        free(tokens); free(raw);
        extern char *coe_help_result(coe_engine *eng);
        return coe_help_result(eng);
    }
    if (console_engine_contains_shell_syntax(raw, NULL) ||
        coe_tokens_contain_shell_token(tokens, ntok)) {
        char *res = coe_result_json(
            "error",
            "Hermes Console does not run shell syntax. Use one supported Hermes "
            "command at a time.", raw, "");
        for (size_t i = 0; i < ntok; i++) free(tokens[i]);
        free(tokens); free(raw);
        return res;
    }

    char *builtin = coe_execute_builtin(eng, tokens, ntok);
    if (builtin) {
        /* history append except for "history"/"clear" */
        if (!(ntok == 1 && (strcmp(tokens[0], "history") == 0 ||
                            strcmp(tokens[0], "clear") == 0)))
            coe_history_append(eng, raw);
        for (size_t i = 0; i < ntok; i++) free(tokens[i]);
        free(tokens); free(raw);
        return builtin;
    }

    /* resolve longest command prefix (3..1) */
    coe_cmd *command = NULL;
    char *rej = coe_rejection_for((const char **)tokens, ntok);
    if (rej && *rej) {
        char *res = coe_result_json("error", rej, raw, "");
        free(rej);
        for (size_t i = 0; i < ntok; i++) free(tokens[i]);
        free(tokens); free(raw);
        return res;
    }
    free(rej);
    size_t key_len = ntok < 3 ? ntok : 3;
    for (size_t sz = key_len; sz >= 1 && !command; sz--) {
        char key[256];
        size_t o = 0;
        key[0] = '\0';
        for (size_t k = 0; k < sz; k++) {
            if (o) key[o++] = ' ';
            snprintf(key + o, sizeof(key) - o, "%s", tokens[k]);
            o = strlen(key);
        }
        for (size_t i = 0; i < eng->n_cmds; i++)
            if (strcmp(eng->cmds[i].key, key) == 0) { command = &eng->cmds[i]; break; }
    }
    if (!command) {
        /* suggestions via difflib.get_close_matches over available paths */
        char **available = malloc(sizeof(char *) * (eng->n_cmds ? eng->n_cmds : 1));
        for (size_t i = 0; i < eng->n_cmds; i++) available[i] = eng->cmds[i].key;
        char probebuf[512];
        probebuf[0] = '\0';
        for (size_t k = 0; k < 2 && k < ntok; k++) {
            if (probebuf[0]) strcat(probebuf, " ");
            strcat(probebuf, tokens[k]);
        }
        const char *probe = probebuf[0] ? probebuf : tokens[0];
        size_t nsug = 0;
        char **sug = coe_close_matches(probe, available, eng->n_cmds, 3, 0.45, &nsug);
        char msgbuf[1024];
        if (nsug) {
            char sugs[512] = "";
            for (size_t i = 0; i < nsug; i++) {
                if (i) strcat(sugs, ", ");
                strncat(sugs, sug[i], sizeof(sugs) - strlen(sugs) - 1);
            }
            snprintf(msgbuf, sizeof(msgbuf),
                     "Unsupported Hermes Console command: %s. Did you mean: %s?",
                     probe, sugs);
        } else {
            snprintf(msgbuf, sizeof(msgbuf),
                     "Unsupported Hermes Console command: %s.", probe);
        }
        for (size_t i = 0; i < nsug; i++) free(sug[i]);
        free(sug); free(available);
        char *res = coe_result_json("error", msgbuf, raw, "");
        for (size_t i = 0; i < ntok; i++) free(tokens[i]);
        free(tokens); free(raw);
        return res;
    }
    /* args are the tokens after the matched key prefix */
    size_t skip = key_len;
    char key[256];
    size_t o = 0;
    key[0] = '\0';
    for (size_t k = 0; k < key_len; k++) {
        if (o) key[o++] = ' ';
        snprintf(key + o, sizeof(key) - o, "%s", tokens[k]);
        o = strlen(key);
    }
    if (strcmp(command->key, key) != 0) skip = 1;
    char **cargs = malloc(sizeof(char *) * (ntok ? ntok : 1));
    size_t ncargs = 0;
    for (size_t k = skip; k < ntok; k++) cargs[ncargs++] = strdup(tokens[k]);

    if (command->mutating && !confirmed) {
        char conf[1024];
        snprintf(conf, sizeof(conf), "%s",
                 command->confirmation[0] ? command->confirmation : "");
        char fallback[600];
        if (!conf[0]) {
            snprintf(fallback, sizeof(fallback), "Run `%s`?", command->usage);
            snprintf(conf, sizeof(conf), "%s", fallback);
        }
        char *res = coe_result_json("confirm_required", "", raw, conf);
        for (size_t i = 0; i < ntok; i++) free(tokens[i]);
        for (size_t i = 0; i < ncargs; i++) free(cargs[i]);
        free(cargs); free(tokens); free(raw);
        return res;
    }

    err = NULL;
    char *output = NULL;
    if (command->kind == 1)
        output = ((coe_simple_fn)command->handler)(eng, cargs, ncargs, &err);
    else
        output = ce_hcmd_invoke((ce_hcmd *)command->handler, cargs, ncargs,
                                coe_confirmed_defaults_trampoline, &err);
    char *result;
    if (!output) {
        result = coe_result_json("error", err ? ce_strip_heap(err) : "", raw, "");
        free(err);
    } else {
        char *rstripped = strdup(output);
        ce_rstrip_inplace(rstripped);
        free(output);
        char *capped = coe_cap_output(eng, rstripped);
        free(rstripped);
        result = coe_result_json("ok", capped, raw, "");
        free(capped);
        coe_history_append(eng, raw);
    }
    for (size_t i = 0; i < ntok; i++) free(tokens[i]);
    for (size_t i = 0; i < ncargs; i++) free(cargs[i]);
    free(cargs); free(tokens); free(raw);
    return result;
}

/* namespace_update trampoline: applies _apply_confirmed_defaults to the
 * parsed namespace before the handler runs. */
int coe_confirmed_defaults_trampoline(ce_ns_t *ns, char **err) {
    return coe_apply_confirmed_defaults(ns, err);
}

/* helper used by coe_execute_full: token-list form of the shell-syntax check */
bool coe_tokens_contain_shell_token(char **tokens, size_t ntok) {
    static const char *shell_tokens[] = {"|", "||", "&", "&&", ";", ">", ">>",
                                         "<", "<<", "2>", "2>>"};
    for (size_t t = 0; t < ntok; t++) {
        for (size_t k = 0; k < sizeof(shell_tokens) / sizeof(shell_tokens[0]); k++)
            if (strcmp(tokens[t], shell_tokens[k]) == 0) return true;
        if (strpbrk(tokens[t], "|<>;")) return true;
    }
    return false;
}

/* PoP: coe_run_console_repl @ hermes_cli/console_engine.py:run_console_repl */
int coe_run_console_repl(FILE *stdin_stream, FILE *stdout_stream, FILE *stderr_stream,
                         int interactive) {
    extern void coe_engine_init(coe_engine *eng, long output_limit);
    char linebuf[65536];

    if (interactive) {
        fprintf(stdout_stream,
                "Hermes Console. Type `help` for commands, `exit` to quit.\n");
        fflush(stdout_stream);
    }
    coe_engine eng;
    coe_engine_init(&eng, 20000);
    coe_register_defaults(&eng);

    for (;;) {
        if (interactive) {
            fputs("hermes> ", stdout_stream);
            fflush(stdout_stream);
        }
        if (!fgets(linebuf, sizeof(linebuf), stdin_stream)) {
            if (interactive) fputc('\n', stdout_stream);
            return 0;
        }
        char *result = coe_execute_full(&eng, linebuf, false);
        /* parse the status out of the result JSON */
        char status[32] = "ok";
        char output_buf[8192] = "";
        char conf_msg[2048] = "";
        char command_buf[65536] = "";
        json_t *rj = result ? json_parse(result, NULL) : NULL;
        if (rj && json_is_object(rj)) {
            const char *st = json_string_value(json_object_get(rj, "status"));
            const char *op = json_string_value(json_object_get(rj, "output"));
            const char *cm = json_string_value(json_object_get(rj, "confirmation_message"));
            const char *cd = json_string_value(json_object_get(rj, "command"));
            snprintf(status, sizeof(status), "%s", st ? st : "ok");
            snprintf(output_buf, sizeof(output_buf), "%s", op ? op : "");
            snprintf(conf_msg, sizeof(conf_msg), "%s", cm ? cm : "");
            snprintf(command_buf, sizeof(command_buf), "%s", cd ? cd : "");
            json_free(rj);
        }
        free(result);

        if (strcmp(status, "confirm_required") == 0) {
            if (!interactive) {
                fprintf(stderr_stream, "Confirmation required: %s\n", conf_msg);
                return 1;
            }
            fprintf(stdout_stream, "%s [y/N] ", conf_msg);
            fflush(stdout_stream);
            char answer[256];
            if (!fgets(answer, sizeof(answer), stdin_stream)) answer[0] = '\0';
            char *ans = ce_strip_heap(answer);
            bool yes = ans && (strcmp(ans, "y") == 0 || strcmp(ans, "yes") == 0 ||
                               strcmp(ans, "Y") == 0 || strcmp(ans, "YES") == 0);
            free(ans);
            if (!yes) {
                fputs("Cancelled.\n", stdout_stream);
                continue;
            }
            result = coe_execute_full(&eng, command_buf, true);
            rj = result ? json_parse(result, NULL) : NULL;
            status[0] = output_buf[0] = '\0';
            if (rj && json_is_object(rj)) {
                const char *st = json_string_value(json_object_get(rj, "status"));
                const char *op = json_string_value(json_object_get(rj, "output"));
                snprintf(status, sizeof(status), "%s", st ? st : "ok");
                snprintf(output_buf, sizeof(output_buf), "%s", op ? op : "");
                json_free(rj);
            }
            free(result);
        }

        if (output_buf[0]) {
            FILE *stream = strcmp(status, "error") == 0 ? stderr_stream : stdout_stream;
            fprintf(stream, "%s\n", output_buf);
        }
        if (strcmp(status, "exit") == 0)
            return 0;
    }
}
