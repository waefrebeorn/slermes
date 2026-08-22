/*
 * t_port_verify_cmd.c — parity harness for src/hermes_cli/verify_cmd.c
 * (hermes_cli/verify_cmd.py). Fixture ops:
 *  {"kind":"merge","recipe":{...},"commands":[...]}
 *      -> {"test":[unioned list]}
 *  {"kind":"run","path_ok":b,"root":...,"recipe":{...}|null,"source":...,
 *   "facts_commands":[...],"args":{...},"result":{...}|null,
 *   "manifest":"<path>","session_id":"s1"}
 *      -> one line with exit code + captured stdout/stderr + hook effects.
 * Mirrors tests/sta_oracle_verify_cmd.py.
 */
#include "hermes_json.h"
#include "verify_cmd.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot read %s\n", path); exit(1); }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)n + 1);
    size_t r = fread(buf, 1, (size_t)n, f); buf[r] = '\0';
    fclose(f);
    return buf;
}

/* capture stdout+stderr of a callback by re-running it after redirecting */
typedef struct { char **lines; size_t n, cap; } capbuf_t;
static void cap_add(capbuf_t *b, const char *s) {
    if (b->n == b->cap) { b->cap = b->cap ? b->cap * 2 : 32;
        b->lines = realloc(b->lines, b->cap * sizeof(char *)); }
    b->lines[b->n++] = strdup(s);
}

static const char *arr_str(json_t *a, size_t i) {
    json_t *e = json_get(a, i);
    return (e && e->type == JSON_STRING) ? e->str_val : "";
}
static vc_recipe_t *recipe_from_json(json_t *j) {
    if (!j || j->type == JSON_NULL) return NULL;
    vc_recipe_t *r = vc_recipe_new();
    r->name = strdup(json_get_str(j, "name", ""));
    r->kind = strdup(json_get_str(j, "kind", ""));
    r->start = strdup(json_get_str(j, "start", ""));
    r->port = (long)json_get_num(j, "port", 0);
    json_t *lists[3] = {json_object_get(j, "bootstrap"),
                        json_object_get(j, "build"),
                        json_object_get(j, "test")};
    char ***ptrs[3] = {&r->bootstrap, &r->build, &r->test};
    size_t *cnts[3] = {&r->n_bootstrap, &r->n_build, &r->n_test};
    for (int L = 0; L < 3; L++) {
        size_t n = (lists[L] && lists[L]->type == JSON_ARRAY) ? json_len(lists[L]) : 0;
        *cnts[L] = n;
        *ptrs[L] = malloc((n ? n : 1) * sizeof(char *));
        for (size_t i = 0; i < n; i++)
            (*ptrs[L])[i] = strdup(arr_str(lists[L], i));
    }
    return r;
}

/* fixture state shared by hooks */
typedef struct {
    int path_ok;
    vc_recipe_t *recipe;
    const char *source;
    const char *const *facts; size_t n_facts;
    const char *manifest;
    vc_result_t *result;
    char *saved_to;
    char *evidence_scope, *evidence_output;
    int recorded;
} fstate_t;

static int h_load(const char *root, vc_recipe_t **out, const char **src, void *ctx) {
    (void)root;
    fstate_t *st = (fstate_t *)ctx;
    if (!st->recipe) return 1;
    *out = st->recipe;
    *src = st->source;
    return 0;
}
static const char *h_manifest_path(const char *root, void *ctx) {
    (void)ctx;
    static char buf[512];
    snprintf(buf, sizeof buf, "%s/.hermes/environment.json", root ? root : ".");
    return buf;
}
static const char *h_save(const char *root, vc_recipe_t *recipe, void *ctx) {
    (void)root; (void)recipe;
    fstate_t *st = (fstate_t *)ctx;
    free(st->saved_to);
    st->saved_to = strdup(st->manifest);
    return st->saved_to;
}
static void h_run(const char *root, vc_recipe_t *recipe,
                  const char *const *phases, size_t n_phases,
                  double pt, double rt, int skip, long port_ov,
                  vc_result_t **out, void *ctx) {
    (void)root; (void)recipe; (void)pt; (void)rt; (void)skip; (void)port_ov;
    /* expose observed phase filter through result identity only —
     * the twin mirrors the same call */
    (void)phases; (void)n_phases;
    fstate_t *st = (fstate_t *)ctx;
    *out = st->result;
}
static const char *const *h_facts(const char *root, size_t *n, void *ctx) {
    (void)root;
    fstate_t *st = (fstate_t *)ctx;
    *n = st->n_facts;
    return st->facts;
}
static void h_record(const char *root, const char *sid, int ok,
                     const char *command, const char *scope,
                     const char *output, void *ctx) {
    (void)root; (void)sid; (void)ok; (void)command;
    fstate_t *st = (fstate_t *)ctx;
    free(st->evidence_scope); free(st->evidence_output);
    st->evidence_scope = strdup(scope);
    st->evidence_output = strdup(output);
    st->recorded++;
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <fixture>\n", argv[0]); return 1; }
    char *raw = read_file(argv[1]);
    json_t *rootj = json_parse(raw, NULL);
    if (!rootj) return 1;
    json_t *ops = json_object_get(rootj, "ops");
    for (size_t i = 0; ops && i < json_len(ops); i++) {
        json_t *c = json_get(ops, i);
        const char *kind = json_get_str(c, "kind", "");
        json_t *o = json_new_object();
        if (strcmp(kind, "merge") == 0) {
            vc_recipe_t *r = recipe_from_json(json_object_get(c, "recipe"));
            json_t *cmds = json_object_get(c, "commands");
            size_t n = cmds && cmds->type == JSON_ARRAY ? json_len(cmds) : 0;
            const char **cv = malloc((n ? n : 1) * sizeof(char *));
            for (size_t k = 0; k < n; k++)
                cv[k] = arr_str(cmds, k);
            verify_cmd_merge_project_facts_commands(r, cv, n);
            json_t *arr = json_new_array();
            for (size_t k = 0; k < r->n_test; k++)
                json_array_append(arr, json_string(r->test[k]));
            json_object_set(o, "test", arr);
            free(cv);
            vc_recipe_free(r);
        } else if (strcmp(kind, "run") == 0) {
            static fstate_t st; memset(&st, 0, sizeof st);
            st.path_ok = json_get_bool(c, "path_ok", 1);
            st.recipe = recipe_from_json(json_object_get(c, "recipe"));
            st.source = json_get_str(c, "source", "detected");
            st.manifest = json_get_str(c, "manifest", "/proj/.hermes/environment.json");
            json_t *fc = json_object_get(c, "facts_commands");
            st.n_facts = fc && fc->type == JSON_ARRAY ? json_len(fc) : 0;
            const char **fv = calloc(st.n_facts ? st.n_facts : 1, sizeof(char *));
            for (size_t k = 0; k < st.n_facts; k++)
                fv[k] = arr_str(fc, k);
            st.facts = fv;
            st.result = NULL;
            json_t *resj = json_object_get(c, "result");
            if (resj && resj->type != JSON_NULL) {
                st.result = vc_result_new();
                st.result->ok = json_get_bool(resj, "ok", 0);
                json_t *ph = json_object_get(resj, "phases");
                st.result->n_phases = ph && ph->type == JSON_ARRAY ? json_len(ph) : 0;
                st.result->phases = calloc(st.result->n_phases ? st.result->n_phases : 1,
                                           sizeof(vc_phase_t));
                for (size_t k = 0; k < st.result->n_phases; k++) {
                    json_t *pj = json_get(ph, k);
                    vc_phase_t *p = &st.result->phases[k];
                    p->phase = strdup(json_get_str(pj, "phase", ""));
                    p->command = strdup(json_get_str(pj, "command", ""));
                    p->ok = json_get_bool(pj, "ok", 0);
                    p->timed_out = json_get_bool(pj, "timed_out", 0);
                    p->duration = json_get_num(pj, "duration", 0.0);
                    p->output_tail = strdup(json_get_str(pj, "output_tail", ""));
                }
                json_t *rj = json_object_get(resj, "readiness");
                if (rj && rj->type != JSON_NULL) {
                    vc_readiness_t *rd = &st.result->readiness;
                    rd->present = 1;
                    rd->ready = json_get_bool(rj, "ready", 0);
                    rd->status_code = (long)json_get_num(rj, "status_code", 0);
                    rd->url = strdup(json_get_str(rj, "url", ""));
                    rd->error = strdup(json_get_str(rj, "error", ""));
                    rd->output_tail = strdup(json_get_str(rj, "output_tail", ""));
                    rd->duration = json_get_num(rj, "duration", 0.0);
                }
            }

            json_t *aj = json_object_get(c, "args");
            verify_cmd_args_t va;
            memset(&va, 0, sizeof va);
            va.path = json_get_str(aj, "path", "");
            va.path_ok = json_get_bool(c, "path_ok", 1);
            va.json = json_get_bool(aj, "json", 0);
            va.save = json_get_bool(aj, "save", 0);
            va.detect_only = json_get_bool(aj, "detect_only", 0);
            va.skip_start = json_get_bool(aj, "skip_start", 0);
            va.port = (long)json_get_num(aj, "port", 0);
            json_t *pj = json_object_get(aj, "phases");
            va.n_phases = pj && pj->type == JSON_ARRAY ? json_len(pj) : 0;
            const char **pv = calloc(va.n_phases ? va.n_phases : 1, sizeof(char *));
            for (size_t k = 0; k < va.n_phases; k++)
                pv[k] = arr_str(pj, k);
            va.phases = pv;
            va.timeout = json_get_num(aj, "timeout", 120.0);
            va.ready_timeout = json_get_num(aj, "ready_timeout", 30.0);

            verify_cmd_hooks_t hk;
            memset(&hk, 0, sizeof hk);
            hk.load_or_detect = h_load;
            hk.manifest_path = h_manifest_path;
            hk.save_manifest = h_save;
            hk.run_verify = h_run;
            hk.detect_facts_commands = h_facts;
            hk.record_verify_run = h_record;
            hk.ctx = &st;

            /* run with redirected stdout/stderr capture */
            fflush(NULL);
            char tf[128]; snprintf(tf, sizeof tf, ".vc_cap_out_%zu", i);
            char tf2[128]; snprintf(tf2, sizeof tf2, ".vc_cap_err_%zu", i);
            FILE *fo = fopen(tf, "w+"), *fe = fopen(tf2, "w+");
            int oldo = dup(fileno(stdout)), olde = dup(fileno(stderr));
            freopen(tf, "w", stdout); freopen(tf2, "w", stderr);
            int rc = verify_cmd_run_verify_command(&va, &hk,
                                                   json_get_str(c, "session_id", ""));
            fflush(NULL);
            dup2(oldo, fileno(stdout)); dup2(olde, fileno(stderr));
            close(oldo); close(olde);

            json_t *so = json_new_array(), *se = json_new_array();
            char line[4096];
            rewind(fo);
            while (fgets(line, sizeof line, fo)) {
                size_t l = strlen(line);
                while (l && (line[l-1]=='\n'||line[l-1]=='\r')) line[--l]=0;
                json_array_append(so, json_string(line));
            }
            rewind(fe);
            while (fgets(line, sizeof line, fe)) {
                size_t l = strlen(line);
                while (l && (line[l-1]=='\n'||line[l-1]=='\r')) line[--l]=0;
                json_array_append(se, json_string(line));
            }
            fclose(fo); fclose(fe);
            remove(tf); remove(tf2);

            json_object_set(o, "exit_code", json_int(rc));
            json_object_set(o, "stdout", so);
            json_object_set(o, "stderr", se);
            json_object_set(o, "record_count", json_int(st.recorded));
            json_object_set(o, "scope",
                json_string(st.evidence_scope ? st.evidence_scope : ""));
            json_object_set(o, "evidence_output",
                json_string(st.evidence_output ? st.evidence_output : ""));
            json_object_set(o, "saved_to",
                json_string(st.saved_to ? st.saved_to : ""));
            free(fv); free(pv);
        }
        char *line = json_dumps(o, 0);
        printf("%s\n", line);
        free(line);
        json_free(o);
    }
    json_free(rootj);
    free(raw);
    return 0;
}
