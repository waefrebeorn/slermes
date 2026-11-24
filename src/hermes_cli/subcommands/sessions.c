/*
 * sessions.c - real C11 port of upstream Hermes hermes_cli/subcommands/sessions.py.
 * That module is a parser specification, so the port carries the whole argparse tree as a static
 * table (subcommand -> ordered argument descriptors) plus the lookups the CLI dispatch needs.
 * Argument tables below are generated from the live argparse tree and match it field for field.
 *
 * PoP: w19_sessions_9adadc_spec @ hermes_cli/subcommands/sessions.py:build_sessions_parser
 * PoP: w19_sessions_9adadc_sub_names @ hermes_cli/subcommands/sessions.py:build_sessions_parser
 * PoP: w19_sessions_9adadc_args @ hermes_cli/subcommands/sessions.py:build_sessions_parser
 * PoP: w19_sessions_9adadc_arg @ hermes_cli/subcommands/sessions.py:build_sessions_parser
 * PoP: w19_sessions_9adadc_arg_count @ hermes_cli/subcommands/sessions.py:build_sessions_parser
 * PoP: w19_sessions_9adadc_parent_args @ hermes_cli/subcommands/sessions.py:build_sessions_parser
 * PoP: w19_sessions_9adadc_dest @ hermes_cli/subcommands/sessions.py:build_sessions_parser
 * PoP: w19_sessions_9adadc_has_sub @ hermes_cli/subcommands/sessions.py:build_sessions_parser
 */
#include "hermes_json.h"
#include "noc_w19_sessions_9adadc.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *opts;      /* comma-joined option strings, "" for a positional */
    const char *dest;
    const char *action;    /* argparse Action class name */
    const char *nargs;     /* NULL, "0", "?", "+", "*", "A..." */
    const char *defval;    /* "N" null, "S" SUPPRESS, "b:true", "i:20", "f:0.5", "s:x", "T:Path" */
    int required;
    const char *type;
    const char *metavar;
    const char *choices;   /* comma-joined, NULL when unset */
} w19_sa_arg_t;

typedef struct {
    const char *name;
    const w19_sa_arg_t *args;
    int n_args;
} w19_sa_sub_t;

static const char *PARENT_DEST = "sessions_action";

static const w19_sa_arg_t PARENT_ARGS[] = {
    {"-h,--help", "help", "_HelpAction", "0", "S", 0, NULL, NULL, NULL},
};
#define PARENT_N 1

static const w19_sa_arg_t ARGS_0[] = {
    {"-h,--help", "help", "_HelpAction", "0", "S", 0, NULL, NULL, NULL},
    {"--source", "source", "_StoreAction", NULL, "N", 0, NULL, NULL, NULL},
    {"--limit", "limit", "_StoreAction", NULL, "i:20", 0, "int", NULL, NULL},
    {"--workspace", "workspace", "_StoreAction", NULL, "N", 0, NULL, "NEEDLE", NULL},
};

static const w19_sa_arg_t ARGS_1[] = {
    {"-h,--help", "help", "_HelpAction", "0", "S", 0, NULL, NULL, NULL},
    {"", "output", "_StoreAction", "?", "N", 0, NULL, "OUTPUT", NULL},
    {"--format", "format", "_StoreAction", NULL, "s:jsonl", 0, NULL, NULL, "jsonl,md,qmd,html,trace"},
    {"--upload", "upload", "_StoreTrueAction", "0", "b:false", 0, NULL, NULL, NULL},
    {"--public", "public", "_StoreTrueAction", "0", "b:false", 0, NULL, NULL, NULL},
    {"--no-redact", "no_redact", "_StoreTrueAction", "0", "b:false", 0, NULL, NULL, NULL},
    {"--only", "only", "_StoreAction", NULL, "N", 0, NULL, NULL, "user-prompts"},
    {"--session-id", "session_id", "_StoreAction", NULL, "N", 0, NULL, NULL, NULL},
    {"--older-than", "older_than", "_StoreAction", NULL, "N", 0, NULL, "AGE", NULL},
    {"--newer-than", "newer_than", "_StoreAction", NULL, "N", 0, NULL, "AGE", NULL},
    {"--before", "before", "_StoreAction", NULL, "N", 0, NULL, "TIME", NULL},
    {"--after", "after", "_StoreAction", NULL, "N", 0, NULL, "TIME", NULL},
    {"--source", "source", "_StoreAction", NULL, "N", 0, NULL, NULL, NULL},
    {"--title", "title", "_StoreAction", NULL, "N", 0, NULL, NULL, NULL},
    {"--end-reason", "end_reason", "_StoreAction", NULL, "N", 0, NULL, NULL, NULL},
    {"--cwd", "cwd", "_StoreAction", NULL, "N", 0, NULL, NULL, NULL},
    {"--min-messages", "min_messages", "_StoreAction", NULL, "N", 0, "int", NULL, NULL},
    {"--max-messages", "max_messages", "_StoreAction", NULL, "N", 0, "int", NULL, NULL},
    {"--model", "model", "_StoreAction", NULL, "N", 0, NULL, NULL, NULL},
    {"--provider", "provider", "_StoreAction", NULL, "N", 0, NULL, NULL, NULL},
    {"--user", "user", "_StoreAction", NULL, "N", 0, NULL, NULL, NULL},
    {"--chat-id", "chat_id", "_StoreAction", NULL, "N", 0, NULL, NULL, NULL},
    {"--chat-type", "chat_type", "_StoreAction", NULL, "N", 0, NULL, NULL, NULL},
    {"--branch", "branch", "_StoreAction", NULL, "N", 0, NULL, NULL, NULL},
    {"--min-tokens", "min_tokens", "_StoreAction", NULL, "N", 0, "int", NULL, NULL},
    {"--max-tokens", "max_tokens", "_StoreAction", NULL, "N", 0, "int", NULL, NULL},
    {"--min-cost", "min_cost", "_StoreAction", NULL, "N", 0, "float", NULL, NULL},
    {"--max-cost", "max_cost", "_StoreAction", NULL, "N", 0, "float", NULL, NULL},
    {"--min-tool-calls", "min_tool_calls", "_StoreAction", NULL, "N", 0, "int", NULL, NULL},
    {"--max-tool-calls", "max_tool_calls", "_StoreAction", NULL, "N", 0, "int", NULL, NULL},
    {"--dry-run", "dry_run", "_StoreTrueAction", "0", "b:false", 0, NULL, NULL, NULL},
    {"--yes,-y", "yes", "_StoreTrueAction", "0", "b:false", 0, NULL, NULL, NULL},
    {"--redact", "redact", "_StoreTrueAction", "0", "b:false", 0, NULL, NULL, NULL},
    {"--lineage", "lineage", "_StoreAction", NULL, "s:single", 0, NULL, NULL, "single,logical"},
    {"--delete-after-verified", "delete_after_verified", "_StoreTrueAction", "0", "b:false", 0, NULL, NULL, NULL},
    {"--force", "force", "_StoreTrueAction", "0", "b:false", 0, NULL, NULL, NULL},
};

static const w19_sa_arg_t ARGS_2[] = {
    {"-h,--help", "help", "_HelpAction", "0", "S", 0, NULL, NULL, NULL},
    {"", "session_id", "_StoreAction", NULL, "N", 1, NULL, NULL, NULL},
    {"--yes,-y", "yes", "_StoreTrueAction", "0", "b:false", 0, NULL, NULL, NULL},
};

static const w19_sa_arg_t ARGS_3[] = {
    {"-h,--help", "help", "_HelpAction", "0", "S", 0, NULL, NULL, NULL},
    {"--older-than", "older_than", "_StoreAction", NULL, "N", 0, NULL, "AGE", NULL},
    {"--newer-than", "newer_than", "_StoreAction", NULL, "N", 0, NULL, "AGE", NULL},
    {"--before", "before", "_StoreAction", NULL, "N", 0, NULL, "TIME", NULL},
    {"--after", "after", "_StoreAction", NULL, "N", 0, NULL, "TIME", NULL},
    {"--source", "source", "_StoreAction", NULL, "N", 0, NULL, NULL, NULL},
    {"--title", "title", "_StoreAction", NULL, "N", 0, NULL, NULL, NULL},
    {"--end-reason", "end_reason", "_StoreAction", NULL, "N", 0, NULL, NULL, NULL},
    {"--cwd", "cwd", "_StoreAction", NULL, "N", 0, NULL, NULL, NULL},
    {"--min-messages", "min_messages", "_StoreAction", NULL, "N", 0, "int", NULL, NULL},
    {"--max-messages", "max_messages", "_StoreAction", NULL, "N", 0, "int", NULL, NULL},
    {"--model", "model", "_StoreAction", NULL, "N", 0, NULL, NULL, NULL},
    {"--provider", "provider", "_StoreAction", NULL, "N", 0, NULL, NULL, NULL},
    {"--user", "user", "_StoreAction", NULL, "N", 0, NULL, NULL, NULL},
    {"--chat-id", "chat_id", "_StoreAction", NULL, "N", 0, NULL, NULL, NULL},
    {"--chat-type", "chat_type", "_StoreAction", NULL, "N", 0, NULL, NULL, NULL},
    {"--branch", "branch", "_StoreAction", NULL, "N", 0, NULL, NULL, NULL},
    {"--min-tokens", "min_tokens", "_StoreAction", NULL, "N", 0, "int", NULL, NULL},
    {"--max-tokens", "max_tokens", "_StoreAction", NULL, "N", 0, "int", NULL, NULL},
    {"--min-cost", "min_cost", "_StoreAction", NULL, "N", 0, "float", NULL, NULL},
    {"--max-cost", "max_cost", "_StoreAction", NULL, "N", 0, "float", NULL, NULL},
    {"--min-tool-calls", "min_tool_calls", "_StoreAction", NULL, "N", 0, "int", NULL, NULL},
    {"--max-tool-calls", "max_tool_calls", "_StoreAction", NULL, "N", 0, "int", NULL, NULL},
    {"--dry-run", "dry_run", "_StoreTrueAction", "0", "b:false", 0, NULL, NULL, NULL},
    {"--yes,-y", "yes", "_StoreTrueAction", "0", "b:false", 0, NULL, NULL, NULL},
    {"--include-archived", "include_archived", "_StoreTrueAction", "0", "b:false", 0, NULL, NULL, NULL},
    {"--include-pinned", "include_pinned", "_StoreTrueAction", "0", "b:false", 0, NULL, NULL, NULL},
    {"--never-active", "never_active", "_StoreTrueAction", "0", "b:false", 0, NULL, NULL, NULL},
    {"--force", "force", "_StoreTrueAction", "0", "b:false", 0, NULL, NULL, NULL},
};

static const w19_sa_arg_t ARGS_4[] = {
    {"-h,--help", "help", "_HelpAction", "0", "S", 0, NULL, NULL, NULL},
    {"--older-than", "older_than", "_StoreAction", NULL, "N", 0, NULL, "AGE", NULL},
    {"--newer-than", "newer_than", "_StoreAction", NULL, "N", 0, NULL, "AGE", NULL},
    {"--before", "before", "_StoreAction", NULL, "N", 0, NULL, "TIME", NULL},
    {"--after", "after", "_StoreAction", NULL, "N", 0, NULL, "TIME", NULL},
    {"--source", "source", "_StoreAction", NULL, "N", 0, NULL, NULL, NULL},
    {"--title", "title", "_StoreAction", NULL, "N", 0, NULL, NULL, NULL},
    {"--end-reason", "end_reason", "_StoreAction", NULL, "N", 0, NULL, NULL, NULL},
    {"--cwd", "cwd", "_StoreAction", NULL, "N", 0, NULL, NULL, NULL},
    {"--min-messages", "min_messages", "_StoreAction", NULL, "N", 0, "int", NULL, NULL},
    {"--max-messages", "max_messages", "_StoreAction", NULL, "N", 0, "int", NULL, NULL},
    {"--model", "model", "_StoreAction", NULL, "N", 0, NULL, NULL, NULL},
    {"--provider", "provider", "_StoreAction", NULL, "N", 0, NULL, NULL, NULL},
    {"--user", "user", "_StoreAction", NULL, "N", 0, NULL, NULL, NULL},
    {"--chat-id", "chat_id", "_StoreAction", NULL, "N", 0, NULL, NULL, NULL},
    {"--chat-type", "chat_type", "_StoreAction", NULL, "N", 0, NULL, NULL, NULL},
    {"--branch", "branch", "_StoreAction", NULL, "N", 0, NULL, NULL, NULL},
    {"--min-tokens", "min_tokens", "_StoreAction", NULL, "N", 0, "int", NULL, NULL},
    {"--max-tokens", "max_tokens", "_StoreAction", NULL, "N", 0, "int", NULL, NULL},
    {"--min-cost", "min_cost", "_StoreAction", NULL, "N", 0, "float", NULL, NULL},
    {"--max-cost", "max_cost", "_StoreAction", NULL, "N", 0, "float", NULL, NULL},
    {"--min-tool-calls", "min_tool_calls", "_StoreAction", NULL, "N", 0, "int", NULL, NULL},
    {"--max-tool-calls", "max_tool_calls", "_StoreAction", NULL, "N", 0, "int", NULL, NULL},
    {"--dry-run", "dry_run", "_StoreTrueAction", "0", "b:false", 0, NULL, NULL, NULL},
    {"--yes,-y", "yes", "_StoreTrueAction", "0", "b:false", 0, NULL, NULL, NULL},
    {"--include-open", "include_open", "_StoreTrueAction", "0", "b:false", 0, NULL, NULL, NULL},
};

static const w19_sa_arg_t ARGS_5[] = {
    {"-h,--help", "help", "_HelpAction", "0", "S", 0, NULL, NULL, NULL},
    {"--force", "force", "_StoreTrueAction", "0", "b:false", 0, NULL, NULL, NULL},
};

static const w19_sa_arg_t ARGS_6[] = {
    {"-h,--help", "help", "_HelpAction", "0", "S", 0, NULL, NULL, NULL},
    {"--dry-run", "dry_run", "_StoreTrueAction", "0", "b:false", 0, NULL, NULL, NULL},
    {"--no-backup", "no_backup", "_StoreTrueAction", "0", "b:false", 0, NULL, NULL, NULL},
};

static const w19_sa_arg_t ARGS_7[] = {
    {"-h,--help", "help", "_HelpAction", "0", "S", 0, NULL, NULL, NULL},
    {"--no-vacuum", "no_vacuum", "_StoreTrueAction", "0", "b:false", 0, NULL, NULL, NULL},
    {"--yes,-y", "yes", "_StoreTrueAction", "0", "b:false", 0, NULL, NULL, NULL},
    {"--force", "force", "_StoreTrueAction", "0", "b:false", 0, NULL, NULL, NULL},
};

static const w19_sa_arg_t ARGS_8[] = {
    {"-h,--help", "help", "_HelpAction", "0", "S", 0, NULL, NULL, NULL},
    {"--check-only", "check_only", "_StoreTrueAction", "0", "b:false", 0, NULL, NULL, NULL},
    {"--no-backup", "no_backup", "_StoreTrueAction", "0", "b:false", 0, NULL, NULL, NULL},
};

static const w19_sa_arg_t ARGS_9[] = {
    {"-h,--help", "help", "_HelpAction", "0", "S", 0, NULL, NULL, NULL},
    {"", "mode", "_StoreAction", NULL, "N", 1, NULL, NULL, "delete,wal"},
    {"--db", "db", "_StoreAction", NULL, "N", 0, NULL, "PATH", NULL},
    {"--force", "force", "_StoreTrueAction", "0", "b:false", 0, NULL, NULL, NULL},
};

static const w19_sa_arg_t ARGS_10[] = {
    {"-h,--help", "help", "_HelpAction", "0", "S", 0, NULL, NULL, NULL},
    {"--apply", "apply", "_StoreTrueAction", "0", "b:false", 0, NULL, NULL, NULL},
    {"--max-gap-seconds", "max_gap_seconds", "_StoreAction", NULL, "N", 0, "float", NULL, NULL},
};

static const w19_sa_arg_t ARGS_11[] = {
    {"-h,--help", "help", "_HelpAction", "0", "S", 0, NULL, NULL, NULL},
    {"--apply", "apply", "_StoreTrueAction", "0", "b:false", 0, NULL, NULL, NULL},
    {"--json", "json", "_StoreTrueAction", "0", "b:false", 0, NULL, NULL, NULL},
    {"--yes,-y", "yes", "_StoreTrueAction", "0", "b:false", 0, NULL, NULL, NULL},
    {"--legacy-main", "legacy_main", "_StoreAction", NULL, "s:report", 0, NULL, NULL, "report,rekey,move"},
};

static const w19_sa_arg_t ARGS_12[] = {
    {"-h,--help", "help", "_HelpAction", "0", "S", 0, NULL, NULL, NULL},
    {"--apply", "apply", "_StoreTrueAction", "0", "b:false", 0, NULL, NULL, NULL},
    {"--json", "json", "_StoreTrueAction", "0", "b:false", 0, NULL, NULL, NULL},
    {"", "session_id", "_StoreAction", "?", "N", 0, NULL, NULL, NULL},
};

static const w19_sa_arg_t ARGS_13[] = {
    {"-h,--help", "help", "_HelpAction", "0", "S", 0, NULL, NULL, NULL},
    {"--session", "session", "_StoreAction", NULL, "N", 0, NULL, "SESSION_ID", NULL},
    {"--json", "json", "_StoreTrueAction", "0", "b:false", 0, NULL, NULL, NULL},
};

static const w19_sa_arg_t ARGS_14[] = {
    {"-h,--help", "help", "_HelpAction", "0", "S", 0, NULL, NULL, NULL},
    {"--source", "source", "_StoreAction", NULL, "N", 1, "Path", NULL, NULL},
    {"--output", "output", "_StoreAction", NULL, "N", 0, "Path", NULL, NULL},
    {"--inspect-only", "inspect_only", "_StoreTrueAction", "0", "b:false", 0, NULL, NULL, NULL},
    {"--work-dir", "work_dir", "_StoreAction", NULL, "N", 0, "Path", NULL, NULL},
    {"--chunk-size", "chunk_size", "_StoreAction", NULL, "i:1000", 0, "int", NULL, NULL},
    {"--allow-partial", "allow_partial", "_StoreTrueAction", "0", "b:false", 0, NULL, NULL, NULL},
    {"--report", "report", "_StoreAction", NULL, "N", 0, "Path", NULL, NULL},
};

static const w19_sa_arg_t ARGS_15[] = {
    {"-h,--help", "help", "_HelpAction", "0", "S", 0, NULL, NULL, NULL},
};

static const w19_sa_arg_t ARGS_16[] = {
    {"-h,--help", "help", "_HelpAction", "0", "S", 0, NULL, NULL, NULL},
    {"", "session_id", "_StoreAction", NULL, "N", 1, NULL, NULL, NULL},
    {"", "title", "_StoreAction", "+", "N", 1, NULL, NULL, NULL},
};

static const w19_sa_arg_t ARGS_17[] = {
    {"-h,--help", "help", "_HelpAction", "0", "S", 0, NULL, NULL, NULL},
    {"", "session", "_StoreAction", NULL, "N", 1, NULL, NULL, NULL},
    {"--admission", "admission", "_AppendAction", NULL, "N", 0, NULL, "ID", NULL},
    {"--yes,-y", "yes", "_StoreTrueAction", "0", "b:false", 0, NULL, NULL, NULL},
};

static const w19_sa_arg_t ARGS_18[] = {
    {"-h,--help", "help", "_HelpAction", "0", "S", 0, NULL, NULL, NULL},
    {"--dry-run", "dry_run", "_StoreTrueAction", "0", "b:false", 0, NULL, NULL, NULL},
};

static const w19_sa_arg_t ARGS_19[] = {
    {"-h,--help", "help", "_HelpAction", "0", "S", 0, NULL, NULL, NULL},
    {"", "session_ids", "_StoreAction", "+", "N", 1, NULL, NULL, NULL},
};

static const w19_sa_arg_t ARGS_20[] = {
    {"-h,--help", "help", "_HelpAction", "0", "S", 0, NULL, NULL, NULL},
    {"", "session_ids", "_StoreAction", "+", "N", 1, NULL, NULL, NULL},
};

static const w19_sa_arg_t ARGS_21[] = {
    {"-h,--help", "help", "_HelpAction", "0", "S", 0, NULL, NULL, NULL},
    {"--json", "json", "_StoreTrueAction", "0", "b:false", 0, NULL, NULL, NULL},
};

static const w19_sa_arg_t ARGS_22[] = {
    {"-h,--help", "help", "_HelpAction", "0", "S", 0, NULL, NULL, NULL},
    {"--apply", "apply", "_StoreTrueAction", "0", "b:false", 0, NULL, NULL, NULL},
    {"--limit", "limit", "_StoreAction", NULL, "i:200", 0, "int", NULL, NULL},
};

static const w19_sa_arg_t ARGS_23[] = {
    {"-h,--help", "help", "_HelpAction", "0", "S", 0, NULL, NULL, NULL},
    {"--source", "source", "_StoreAction", NULL, "N", 0, NULL, NULL, NULL},
    {"--limit", "limit", "_StoreAction", NULL, "i:500", 0, "int", NULL, NULL},
};

static const w19_sa_arg_t ARGS_24[] = {
    {"-h,--help", "help", "_HelpAction", "0", "S", 0, NULL, NULL, NULL},
    {"--from", "from_source", "_StoreAction", NULL, "N", 0, NULL, NULL, "claude,codex"},
    {"", "path", "_StoreAction", "?", "N", 0, NULL, NULL, NULL},
};

static const w19_sa_sub_t SUBS[] = {
    {"list", ARGS_0, 4},
    {"export", ARGS_1, 36},
    {"delete", ARGS_2, 3},
    {"prune", ARGS_3, 29},
    {"archive", ARGS_4, 26},
    {"optimize", ARGS_5, 2},
    {"clean-markers", ARGS_6, 3},
    {"optimize-storage", ARGS_7, 4},
    {"repair", ARGS_8, 3},
    {"set-journal-mode", ARGS_9, 4},
    {"repair-routing", ARGS_10, 3},
    {"repair-profiles", ARGS_11, 5},
    {"repair-prompts", ARGS_12, 4},
    {"refresh-prompts", ARGS_13, 3},
    {"recover", ARGS_14, 8},
    {"stats", ARGS_15, 1},
    {"rename", ARGS_16, 3},
    {"discard", ARGS_17, 4},
    {"adopt", ARGS_18, 2},
    {"pin", ARGS_19, 2},
    {"unpin", ARGS_20, 2},
    {"pinned", ARGS_21, 2},
    {"retitle-skills", ARGS_22, 3},
    {"browse", ARGS_23, 3},
    {"import", ARGS_24, 3},
};
#define SUBS_N 25

static json_t *split_csv(const char *s) {
    json_t *arr = json_array();
    const char *p = s;
    if (!s || !*s) return arr;
    for (;;) {
        const char *c = strchr(p, ',');
        if (c) {
            size_t n = (size_t)(c - p);
            char *t = (char *)malloc(n + 1);
            if (t) {
                memcpy(t, p, n);
                t[n] = '\0';
                json_append(arr, json_string(t));
                free(t);
            }
            p = c + 1;
        } else {
            json_append(arr, json_string(p));
            break;
        }
    }
    return arr;
}

static json_t *defval_node(const char *e) {
    if (!e || !strcmp(e, "N")) return json_null();
    if (!strcmp(e, "S")) return json_string("__SUPPRESS__");
    if (!strncmp(e, "b:", 2)) return json_bool(!strcmp(e + 2, "true"));
    if (!strncmp(e, "i:", 2)) return json_number((double)strtoll(e + 2, NULL, 10));
    if (!strncmp(e, "f:", 2)) return json_number(strtod(e + 2, NULL));
    if (!strncmp(e, "s:", 2)) return json_string(e + 2);
    if (!strncmp(e, "T:", 2)) {
        json_t *o = json_object();
        json_set(o, "__type__", json_string(e + 2));
        return o;
    }
    return json_null();
}

static json_t *arg_node(const w19_sa_arg_t *a) {
    json_t *o = json_object();
    json_set(o, "opts", split_csv(a->opts));
    json_set(o, "dest", a->dest ? json_string(a->dest) : json_null());
    json_set(o, "action", a->action ? json_string(a->action) : json_null());
    json_set(o, "nargs", a->nargs ? (!strcmp(a->nargs, "0") ? json_number(0) : json_string(a->nargs))
                                 : json_null());
    json_set(o, "default", defval_node(a->defval));
    json_set(o, "required", json_bool(a->required != 0));
    json_set(o, "type", a->type ? json_string(a->type) : json_null());
    json_set(o, "metavar", a->metavar ? json_string(a->metavar) : json_null());
    json_set(o, "choices", a->choices ? split_csv(a->choices) : json_null());
    return o;
}

static json_t *args_node(const w19_sa_arg_t *args, int n) {
    json_t *arr = json_array();
    int i;
    for (i = 0; i < n; i++) json_append(arr, arg_node(&args[i]));
    return arr;
}

static const w19_sa_sub_t *find_sub(const char *name) {
    int i;
    if (!name) return NULL;
    for (i = 0; i < SUBS_N; i++) {
        if (!strcmp(SUBS[i].name, name)) return &SUBS[i];
    }
    return NULL;
}

json_t *w19_sessions_9adadc_spec(void) {
    json_t *out = json_object();
    json_t *subs = json_object();
    int i;
    json_set(out, "dest", json_string(PARENT_DEST));
    json_set(out, "parent", args_node(PARENT_ARGS, PARENT_N));
    for (i = 0; i < SUBS_N; i++) {
        json_t *entry = json_object();
        json_set(entry, "args", args_node(SUBS[i].args, SUBS[i].n_args));
        json_set(entry, "children", json_null());
        json_set(subs, SUBS[i].name, entry);
    }
    json_set(out, "subs", subs);
    return out;
}

json_t *w19_sessions_9adadc_parent_args(void) {
    return args_node(PARENT_ARGS, PARENT_N);
}

json_t *w19_sessions_9adadc_sub_names(void) {
    json_t *arr = json_array();
    int i;
    for (i = 0; i < SUBS_N; i++) json_append(arr, json_string(SUBS[i].name));
    return arr;
}

json_t *w19_sessions_9adadc_args(const char *sub) {
    const w19_sa_sub_t *s = find_sub(sub);
    if (!s) return json_null();
    return args_node(s->args, s->n_args);
}

json_t *w19_sessions_9adadc_arg(const char *sub, const char *flag) {
    const w19_sa_sub_t *s = find_sub(sub);
    int i;
    if (!s || !flag) return json_null();
    for (i = 0; i < s->n_args; i++) {
        const char *opts = s->args[i].opts;
        const char *p = opts;
        if (!opts || !*opts) continue;
        for (;;) {
            const char *c = strchr(p, ',');
            size_t n = c ? (size_t)(c - p) : strlen(p);
            if (strlen(flag) == n && !strncmp(p, flag, n)) return arg_node(&s->args[i]);
            if (!c) break;
            p = c + 1;
        }
    }
    return json_null();
}

json_t *w19_sessions_9adadc_arg_count(const char *sub) {
    const w19_sa_sub_t *s = find_sub(sub);
    if (!s) return json_null();
    return json_number((double)s->n_args);
}

const char *w19_sessions_9adadc_dest(void) {
    return PARENT_DEST;
}

int w19_sessions_9adadc_has_sub(const char *name) {
    return find_sub(name) ? 1 : 0;
}
