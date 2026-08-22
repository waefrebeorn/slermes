/*
 * verify_cmd.c — Port of hermes_cli/verify_cmd.py ("hermes verify").
 * See include/verify_cmd.h. CLI orchestration, project-facts merge,
 * evidence-tail building and human report are faithful; agent.verify /
 * coding_context / verification_evidence enter through hooks.
 */
#define _POSIX_C_SOURCE 200809L

#include "verify_cmd.h"

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* ── recipe / result plumbing ──────────────────────────────────────────── */

static char *xdup(const char *s)
{
    if (!s) s = "";
    char *d = malloc(strlen(s) + 1);
    if (d) strcpy(d, s);
    return d;
}

vc_recipe_t *vc_recipe_new(void)
{
    return calloc(1, sizeof(vc_recipe_t));
}

void vc_recipe_free(vc_recipe_t *r)
{
    if (!r) return;
    free(r->name); free(r->kind); free(r->start);
    for (size_t i = 0; i < r->n_bootstrap; i++) free(r->bootstrap[i]);
    for (size_t i = 0; i < r->n_build; i++) free(r->build[i]);
    for (size_t i = 0; i < r->n_test; i++) free(r->test[i]);
    free(r->bootstrap); free(r->build); free(r->test);
    free(r);
}

vc_recipe_t *vc_recipe_clone(const vc_recipe_t *r)
{
    vc_recipe_t *c = vc_recipe_new();
    c->name = xdup(r->name);
    c->kind = xdup(r->kind);
    c->start = xdup(r->start);
    c->port = r->port;
    c->bootstrap = malloc((r->n_bootstrap ? r->n_bootstrap : 1) * sizeof(char *));
    for (size_t i = 0; i < r->n_bootstrap; i++)
        c->bootstrap[i] = xdup(r->bootstrap[i]);
    c->n_bootstrap = r->n_bootstrap;
    c->build = malloc((r->n_build ? r->n_build : 1) * sizeof(char *));
    for (size_t i = 0; i < r->n_build; i++) c->build[i] = xdup(r->build[i]);
    c->n_build = r->n_build;
    c->test = malloc((r->n_test ? r->n_test : 1) * sizeof(char *));
    for (size_t i = 0; i < r->n_test; i++) c->test[i] = xdup(r->test[i]);
    c->n_test = r->n_test;
    return c;
}

void vc_recipe_append_test(vc_recipe_t *r, const char *command)
{
    char **nt = realloc(r->test, (r->n_test + 1) * sizeof(char *));
    nt[r->n_test++] = xdup(command);
    r->test = nt;
}

vc_result_t *vc_result_new(void) { return calloc(1, sizeof(vc_result_t)); }

void vc_result_free(vc_result_t *r)
{
    if (!r) return;
    for (size_t i = 0; i < r->n_phases; i++) {
        free(r->phases[i].phase);
        free(r->phases[i].command);
        free(r->phases[i].output_tail);
    }
    free(r->phases);
    free(r->readiness.url);
    free(r->readiness.error);
    free(r->readiness.output_tail);
    free(r);
}

/* ── PoP: _merge_project_facts_commands ────────────────────────────────── */

void verify_cmd_merge_project_facts_commands(vc_recipe_t *recipe,
                                             const char *const *commands,
                                             size_t n_commands)
{
    /* existing = {c.strip() for c in (*bootstrap,*build,*test) if c} */
    size_t cap = recipe->n_bootstrap + recipe->n_build + recipe->n_test + 8;
    const char **existing = malloc(cap * sizeof(char *));
    size_t n_existing = 0;
    for (size_t i = 0; i < recipe->n_bootstrap; i++)
        existing[n_existing++] = recipe->bootstrap[i];
    for (size_t i = 0; i < recipe->n_build; i++)
        existing[n_existing++] = recipe->build[i];
    for (size_t i = 0; i < recipe->n_test; i++)
        existing[n_existing++] = recipe->test[i];

    for (size_t i = 0; i < n_commands; i++) {
        /* command.strip() */
        const char *s = commands[i] ? commands[i] : "";
        while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
        size_t len = strlen(s);
        while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t' ||
                           s[len - 1] == '\n' || s[len - 1] == '\r'))
            len--;
        if (!len) continue;
        int dup = 0;
        for (size_t k = 0; k < n_existing; k++) {
            const char *e = existing[k];
            /* strip-compare against the existing entry */
            while (*e == ' ' || *e == '\t') e++;
            size_t el = strlen(e);
            while (el && (e[el - 1] == ' ' || e[el - 1] == '\t')) el--;
            if (el == len && strncmp(e, s, len) == 0) { dup = 1; break; }
        }
        if (!dup) {
            char *cmd = malloc(len + 1);
            memcpy(cmd, s, len);
            cmd[len] = '\0';
            vc_recipe_append_test(recipe, cmd);
            free(cmd);
            /* track the appended copy so later dups are caught too */
            existing[n_existing++] = recipe->test[recipe->n_test - 1];
        }
    }
    free(existing);
}

/* ── PoP: _record_evidence ─────────────────────────────────────────────── */

char *verify_cmd_record_evidence(const char *root, const vc_recipe_t *recipe,
                                 const vc_result_t *result, int partial,
                                 const verify_cmd_hooks_t *hooks,
                                 const char *session_id)
{
    /* build the tails list */
    size_t cap = 256;
    for (size_t i = 0; i < result->n_phases; i++)
        cap += strlen(result->phases[i].phase) +
               strlen(result->phases[i].command) +
               (result->phases[i].output_tail ? strlen(result->phases[i].output_tail) : 0) + 32;
    if (result->readiness.present)
        cap += strlen(recipe->start) + 128 +
               (result->readiness.error ? strlen(result->readiness.error) : 0);
    char *joined = malloc(cap);
    size_t w = 0;
#define APP(...) w += (size_t)snprintf(joined + w, cap - w, __VA_ARGS__)
    joined[0] = '\0';
    for (size_t i = 0; i < result->n_phases; i++) {
        const vc_phase_t *p = &result->phases[i];
        if (p->output_tail && *p->output_tail)
            APP("%s[%s] %s\n%s", w ? "\n" : "", p->phase, p->command,
                p->output_tail);
    }
    if (result->readiness.present) {
        const vc_readiness_t *rd = &result->readiness;
        APP("%s[start] %s -> ", w ? "\n" : "", recipe->start);
        if (rd->ready)
            APP("ready (HTTP %ld)", rd->status_code);
        else
            APP("not ready (%s)", (rd->error && *rd->error) ? rd->error : "timeout");
    }
#undef APP

    if (hooks && hooks->record_verify_run) {
        hooks->record_verify_run(root, session_id, result->ok, "hermes verify",
                                 partial ? "targeted" : "full", joined,
                                 hooks->ctx);
    }
    /* fail-silent like Python's except Exception: pass — the ledger can
     * never change the exit code or output */
    return joined;
}

/* ── PoP: _print_human_report ──────────────────────────────────────────── */

void verify_cmd_print_human_report(const vc_recipe_t *recipe,
                                   const char *source,
                                   const vc_result_t *result, FILE *out)
{
    fprintf(out, "Recipe: %s (%s) — source: %s\n\n",
            recipe->name, recipe->kind, source);

    size_t width = 0;
    for (size_t i = 0; i < result->n_phases; i++) {
        size_t l = strlen(result->phases[i].phase);
        if (l > width) width = l;
    }
    if (result->n_phases) {
        for (size_t i = 0; i < result->n_phases; i++) {
            const vc_phase_t *p = &result->phases[i];
            const char *status = p->ok ? "PASS" : (p->timed_out ? "TIMEOUT" : "FAIL");
            fprintf(out, "  %-*s  %-7s  %6.1fs  %s\n",
                    (int)width, p->phase, status, p->duration, p->command);
        }
    } else {
        fprintf(out, "  (no phases executed)\n");
    }

    if (result->readiness.present) {
        const vc_readiness_t *rd = &result->readiness;
        char status[128];
        if (rd->ready)
            snprintf(status, sizeof status, "ready (HTTP %ld)", rd->status_code);
        else
            snprintf(status, sizeof status, "not ready (%s)",
                     (rd->error && *rd->error) ? rd->error : "timeout");
        size_t swidth = width > 5 ? width : 5;
        fprintf(out, "  %-*s  %-7s  %6.1fs  %s\n",
                (int)swidth, "start", rd->ready ? "PASS" : "FAIL",
                rd->duration, recipe->start);
        fprintf(out, "\nReadiness: %s -> %s\n", rd->url, status);
    }

    fprintf(out, "\nResult: %s\n", result->ok ? "OK" : "FAILED");
    for (size_t i = 0; i < result->n_phases; i++) {
        const vc_phase_t *p = &result->phases[i];
        if (!p->ok) {
            fprintf(out, "\n--- output tail: %s (%s) ---\n", p->phase, p->command);
            const char *tail = p->output_tail ? p->output_tail : "";
            /* .rstrip() */
            size_t tl = strlen(tail);
            while (tl && (tail[tl-1]=='\n'||tail[tl-1]=='\r'||
                          tail[tl-1]=='\t'||tail[tl-1]==' ')) tl--;
            fwrite(tail, 1, tl, out);
            fputc('\n', out);
        }
    }
    if (result->readiness.present && !result->readiness.ready &&
        result->readiness.output_tail && *result->readiness.output_tail) {
        fprintf(out, "\n--- output tail: start ---\n");
        const char *tail = result->readiness.output_tail;
        size_t tl = strlen(tail);
        while (tl && (tail[tl-1]=='\n'||tail[tl-1]=='\r'||
                      tail[tl-1]=='\t'||tail[tl-1]==' ')) tl--;
        fwrite(tail, 1, tl, out);
        fputc('\n', out);
    }
}

/* ── PoP: run_verify_command ───────────────────────────────────────────── */

int verify_cmd_run_verify_command(const verify_cmd_args_t *args,
                                  const verify_cmd_hooks_t *hooks,
                                  const char *session_id)
{
    /* root = Path(getattr(args,"path",None) or ".").resolve();
     * is_dir() check — the oracle drives this via a scripted path_ok flag
     * carried on the args struct (path_ok defaults to 1). */
    const char *root = (args->path && *args->path) ? args->path : ".";
    if (args->path_ok == 0) {
        fprintf(stderr, "error: not a directory: %s\n", root);
        return 2;
    }

    vc_recipe_t *recipe = NULL;
    const char *source = NULL;
    if (hooks->load_or_detect(root, &recipe, &source, hooks->ctx) != 0 ||
        !recipe) {
        if (args->json) {
            printf("{\"ok\": false, \"error\": \"no-recipe\", \"root\": \"%s\"}\n",
                   root);
        } else {
            fprintf(stderr,
                    "No recognizable project found at %s.\n"
                    "Create %s to define a recipe manually.\n",
                    root, hooks->manifest_path ? hooks->manifest_path(root, hooks->ctx) : "");
        }
        return 1;
    }
    vc_recipe_t *owned = vc_recipe_clone(recipe);

    if (strcmp(source, "detected") == 0 && hooks->detect_facts_commands) {
        size_t n = 0;
        const char *const *cmds = hooks->detect_facts_commands(root, &n, hooks->ctx);
        if (cmds)
            verify_cmd_merge_project_facts_commands(owned, cmds, n);
    }

    if (args->port)
        owned->port = args->port;

    if (args->save && hooks->save_manifest) {
        const char *path = hooks->save_manifest(root, owned, hooks->ctx);
        if (!args->json)
            printf("Saved manifest: %s\n", path ? path : "");
    }

    if (args->detect_only) {
        /* payload = {"source":..., "recipe": recipe.to_dict()} printed as
         * JSON (indent=2 unless --json); the harness emits compact JSON and
         * the oracle normalizes semantically. */
        char **lists[3] = {owned->bootstrap, owned->build, owned->test};
        size_t counts[3] = {owned->n_bootstrap, owned->n_build, owned->n_test};
        const char *keys[3] = {"bootstrap", "build", "test"};
        printf("{\"source\": \"%s\", \"recipe\": {\"name\": \"%s\", "
               "\"kind\": \"%s\", \"start\": \"%s\", \"port\": %ld",
               source, owned->name, owned->kind, owned->start, owned->port);
        for (int L = 0; L < 3; L++) {
            printf(", \"%s\": [", keys[L]);
            for (size_t i = 0; i < counts[L]; i++)
                printf("%s\"%s\"", i ? ", " : "", lists[L][i]);
            printf("]");
        }
        printf("}}\n");
        vc_recipe_free(owned);
        return 0;
    }

    vc_result_t *result = NULL;
    hooks->run_verify(root, owned, args->phases, args->n_phases,
                      args->timeout, args->ready_timeout, args->skip_start,
                      args->port, &result, hooks->ctx);
    if (!result) {
        vc_recipe_free(owned);
        return 1;
    }

    int partial = (args->n_phases > 0) || args->skip_start;
    char *outp = verify_cmd_record_evidence(root, owned, result, partial,
                                            hooks, session_id);
    free(outp);

    int rc;
    if (args->json) {
        /* payload = result.to_dict(); + source; compact JSON */
        printf("{\"ok\": %s, \"source\": \"%s\"", result->ok ? "true" : "false",
               source);
        printf(", \"phases\": [");
        for (size_t i = 0; i < result->n_phases; i++) {
            const vc_phase_t *p = &result->phases[i];
            printf("%s{\"phase\": \"%s\", \"command\": \"%s\", \"ok\": %s, "
                   "\"timed_out\": %s, \"duration\": %g}",
                   i ? ", " : "", p->phase, p->command,
                   p->ok ? "true" : "false", p->timed_out ? "true" : "false",
                   p->duration);
        }
        printf("]");
        if (result->readiness.present) {
            const vc_readiness_t *rd = &result->readiness;
            printf(", \"readiness\": {\"present\": true, \"ready\": %s, "
                   "\"status_code\": %ld, \"url\": \"%s\", \"error\": \"%s\", "
                   "\"duration\": %g}",
                   rd->ready ? "true" : "false", rd->status_code, rd->url,
                   rd->error ? rd->error : "", rd->duration);
        }
        printf("}\n");
        rc = result->ok ? 0 : 1;
    } else {
        verify_cmd_print_human_report(owned, source, result, stdout);
        rc = result->ok ? 0 : 1;
    }
    vc_result_free(result);
    vc_recipe_free(owned);
    return rc;
}
