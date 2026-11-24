/*
 * codex_runtime.c -- C11 port of upstream Hermes
 *   hermes_cli/subcommands/codex_runtime.py
 *
 * PoP: w21_codexrunti_5cdf58_migrate @ hermes_cli/subcommands/codex_runtime.py:cmd_codex_runtime_migrate
 * PoP: w21_codexrunti_5cdf58_summary @ hermes_cli/codex_runtime_plugin_migration.py:MigrationReport.summary
 * PoP: w21_codexrunti_5cdf58_parser @ hermes_cli/subcommands/codex_runtime.py:build_codex_runtime_parser
 *
 * `hermes codex-runtime migrate` is a projection of a MigrationReport: either
 * MigrationReport.summary() or dataclasses.asdict() + str(target_path) as
 * pretty JSON, with exit 1 on any report error. The migration run itself
 * (codex_runtime_plugin_migration.migrate, which rewrites ~/.codex/config.toml)
 * stays upstream: the report object is the seam, exactly what `migrate`
 * returns. The argparse wiring is a static table; only the sub-command help and
 * the two store_true flags are behaviour (dest/default), so those -- not the
 * prose descriptions -- are what the table carries.
 *
 * U+26A0 in summary() is emitted as a UTF-8 escape so this source stays ASCII.
 */
#include "hermes_json.h"
#include "noc_w21_codexrunti_5cdf58.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CODEXRUNTI_WARN "\xe2\x9a\xa0"

/* --- growable text buffer ------------------------------------------------- */

typedef struct {
    char *p;
    size_t n;
    size_t cap;
} codexrunti_buf;

static void codexrunti_buf_init(codexrunti_buf *b) {
    b->p = NULL;
    b->n = 0;
    b->cap = 0;
}

static void codexrunti_buf_reserve(codexrunti_buf *b, size_t extra) {
    size_t want = b->n + extra + 1;
    char *grown;
    if (want <= b->cap) return;
    if (b->cap == 0) b->cap = 128;
    while (b->cap < want) b->cap *= 2;
    grown = (char *)realloc(b->p, b->cap);
    if (!grown) return;
    b->p = grown;
}

static void codexrunti_buf_add(codexrunti_buf *b, const char *s) {
    size_t n;
    if (!s) s = "";
    n = strlen(s);
    codexrunti_buf_reserve(b, n);
    if (!b->p) return;
    memcpy(b->p + b->n, s, n + 1);
    b->n += n;
}

static void codexrunti_buf_fmt(codexrunti_buf *b, const char *fmt, ...) {
    va_list ap, ap2;
    int need;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    need = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (need < 0) {
        va_end(ap2);
        return;
    }
    codexrunti_buf_reserve(b, (size_t)need);
    if (b->p) {
        vsnprintf(b->p + b->n, b->cap - b->n, fmt, ap2);
        b->n += (size_t)need;
    }
    va_end(ap2);
}

/* --- report field readers ------------------------------------------------- */

static size_t codexrunti_count(const json_t *arr) {
    if (!arr || arr->type != JSON_ARRAY) return 0;
    return arr->c.count;
}

static const char *codexrunti_item_str(const json_t *arr, size_t i) {
    json_t *v = (arr && arr->type == JSON_ARRAY && i < arr->c.count) ? arr->c.items[i] : NULL;
    if (v && v->type == JSON_STRING && v->str_val) return v->str_val;
    return "";
}

static const char *codexrunti_opt_str(const json_t *obj, const char *key) {
    json_t *v = obj ? json_obj_get(obj, key) : NULL;
    if (v && v->type == JSON_STRING && v->str_val) return v->str_val;
    return NULL;
}

/* ", ".join(items) over an array of strings. */
static void codexrunti_join(codexrunti_buf *b, const json_t *arr) {
    size_t i, n = codexrunti_count(arr);
    for (i = 0; i < n; i++) {
        if (i) codexrunti_buf_add(b, ", ");
        codexrunti_buf_add(b, codexrunti_item_str(arr, i));
    }
}

/* f"{self.target_path}" -- str(Path) or "None". */
static void codexrunti_add_target(codexrunti_buf *b, const json_t *report) {
    const char *tp = codexrunti_opt_str(report, "target_path");
    codexrunti_buf_add(b, tp ? tp : "None");
}

/* --- MigrationReport.summary() ------------------------------------------- */

char *w21_codexrunti_5cdf58_summary(const json_t *report) {
    codexrunti_buf b;
    json_t *migrated = json_obj_get(report, "migrated");
    json_t *skipped = json_obj_get(report, "skipped_keys_per_server");
    json_t *plugins = json_obj_get(report, "migrated_plugins");
    json_t *preserved = json_obj_get(report, "preserved_user_servers");
    json_t *errors = json_obj_get(report, "errors");
    const char *plugin_error = codexrunti_opt_str(report, "plugin_query_error");
    const char *permissions = codexrunti_opt_str(report, "wrote_permissions_default");
    int first = 1;
    size_t i, n, kept;

    codexrunti_buf_init(&b);

    if (json_get_bool(report, "dry_run", 0)) {
        codexrunti_buf_add(&b, "(dry run) Would write ");
        codexrunti_add_target(&b, report);
        first = 0;
    } else if (json_get_bool(report, "written", 0)) {
        codexrunti_buf_add(&b, "Wrote ");
        codexrunti_add_target(&b, report);
        first = 0;
    }

    n = codexrunti_count(migrated);
    if (n > 0) {
        if (!first) codexrunti_buf_add(&b, "\n");
        first = 0;
        codexrunti_buf_fmt(&b, "Migrated %zu MCP server(s):", n);
        for (i = 0; i < n; i++) {
            const char *name = codexrunti_item_str(migrated, i);
            json_t *skip = skipped ? json_obj_get(skipped, name) : NULL;
            codexrunti_buf_add(&b, "\n  - ");
            codexrunti_buf_add(&b, name);
            if (codexrunti_count(skip) > 0) {
                codexrunti_buf_add(&b, " (skipped: ");
                codexrunti_join(&b, skip);
                codexrunti_buf_add(&b, ")");
            }
        }
    } else {
        if (!first) codexrunti_buf_add(&b, "\n");
        first = 0;
        codexrunti_buf_add(&b, "No MCP servers found in Hermes config.");
    }

    n = codexrunti_count(plugins);
    if (n > 0) {
        if (!first) codexrunti_buf_add(&b, "\n");
        first = 0;
        codexrunti_buf_fmt(&b, "Migrated %zu native Codex plugin(s):", n);
        for (i = 0; i < n; i++) {
            codexrunti_buf_add(&b, "\n  - ");
            codexrunti_buf_add(&b, codexrunti_item_str(plugins, i));
        }
    } else if (plugin_error && plugin_error[0]) {
        if (!first) codexrunti_buf_add(&b, "\n");
        first = 0;
        codexrunti_buf_fmt(&b, "Codex plugin discovery skipped: %s", plugin_error);
    }

    if (permissions && permissions[0]) {
        if (!first) codexrunti_buf_add(&b, "\n");
        first = 0;
        codexrunti_buf_fmt(&b, "Wrote default_permissions = '%s'", permissions);
    }

    kept = codexrunti_count(preserved);
    if (kept > 0) {
        if (!first) codexrunti_buf_add(&b, "\n");
        first = 0;
        codexrunti_buf_fmt(&b, "Kept %zu user-owned MCP server(s) already in config.toml "
                               "(Hermes projection skipped): ", kept);
        codexrunti_join(&b, preserved);
    }

    n = codexrunti_count(errors);
    for (i = 0; i < n; i++) {
        if (!first) codexrunti_buf_add(&b, "\n");
        first = 0;
        codexrunti_buf_add(&b, CODEXRUNTI_WARN " ");
        codexrunti_buf_add(&b, codexrunti_item_str(errors, i));
    }

    if (!b.p) return codexrunti_buf_init(&b), NULL;
    return b.p;
}

/* --- dataclasses.asdict(report) + str(target_path) ------------------------ */

static json_t *codexrunti_payload(const json_t *report) {
    json_t *out = json_object();
    static const char *const ARRAYS[] = {"migrated", "migrated_plugins",
                                         "preserved_user_servers", "errors", NULL};
    static const char *const OPTIONALS[] = {"plugin_query_error",
                                            "wrote_permissions_default", NULL};
    const char *target = codexrunti_opt_str(report, "target_path");
    int i;

    json_set(out, "target_path", json_string(target ? target : "None"));
    for (i = 0; ARRAYS[i]; i++) {
        json_t *v = json_obj_get(report, ARRAYS[i]);
        json_set(out, ARRAYS[i], (v && v->type == JSON_ARRAY) ? json_copy(v) : json_array());
    }
    {
        json_t *v = json_obj_get(report, "skipped_keys_per_server");
        json_set(out, "skipped_keys_per_server",
                 (v && v->type == JSON_OBJECT) ? json_copy(v) : json_object());
    }
    for (i = 0; OPTIONALS[i]; i++) {
        const char *s = codexrunti_opt_str(report, OPTIONALS[i]);
        json_set(out, OPTIONALS[i], s ? json_string(s) : json_null());
    }
    json_set(out, "written", json_bool(json_get_bool(report, "written", 0)));
    json_set(out, "dry_run", json_bool(json_get_bool(report, "dry_run", 0)));
    return out;
}

/* --- cmd_codex_runtime_migrate ------------------------------------------- */

json_t *w21_codexrunti_5cdf58_migrate(const json_t *state) {
    json_t *report = json_obj_get(state, "report");
    json_t *out = json_object();
    int owned = 0;
    int rc;

    if (!report || report->type != JSON_OBJECT) {
        report = json_object();
        owned = 1;
    }
    rc = codexrunti_count(json_obj_get(report, "errors")) > 0 ? 1 : 0;

    json_set(out, "rc", json_number((double)rc));
    if (json_get_bool(state, "json", 0)) {
        json_set(out, "payload", codexrunti_payload(report));
    } else {
        /* print(report.summary()) -- the newline belongs to print(). */
        char *text = w21_codexrunti_5cdf58_summary(report);
        codexrunti_buf b;
        codexrunti_buf_init(&b);
        codexrunti_buf_add(&b, text ? text : "");
        codexrunti_buf_add(&b, "\n");
        json_set(out, "text", json_string(b.p ? b.p : ""));
        free(b.p);
        free(text);
    }
    if (owned) json_free(report);
    return out;
}

/* --- build_codex_runtime_parser ------------------------------------------ */

static void codexrunti_option(json_t *opts, const char *name, const char *dest,
                              const char *help) {
    json_t *opt = json_object();
    json_t *names = json_array();
    json_append(names, json_string(name));
    json_set(opt, "options", names);
    json_set(opt, "dest", json_string(dest));
    json_set(opt, "action", json_string("_StoreTrueAction"));
    json_set(opt, "default", json_bool(0));
    json_set(opt, "help", json_string(help));
    json_append(opts, opt);
}

json_t *w21_codexrunti_5cdf58_parser(void) {
    json_t *out = json_object();
    json_t *choices = json_array();
    json_t *helps = json_object();
    json_t *subs = json_object();
    json_t *migrate = json_object();
    json_t *opts = json_array();

    json_set(out, "dest", json_string("codex_runtime_action"));
    json_append(choices, json_string("migrate"));
    json_set(out, "choices", choices);
    json_set(out, "aliases", json_object());
    json_set(helps, "migrate",
             json_string("Regenerate the hermes-managed block in codex's config.toml"));
    json_set(out, "helps", helps);
    json_set(out, "func", json_string("_print_help"));

    codexrunti_option(opts, "--dry-run", "dry_run",
                      "Report what would be written without touching config.toml");
    codexrunti_option(opts, "--json", "json",
                      "Print the migration report as JSON (for automation)");
    json_set(migrate, "options", opts);
    json_set(migrate, "func", json_string("cmd_codex_runtime_migrate"));
    json_set(subs, "migrate", migrate);
    json_set(out, "subs", subs);
    return out;
}
