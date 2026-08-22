/*
 * t_port_subagent_worktree.c — parity harness for src/tools/subagent_worktree.c
 * (tools/subagent_worktree.py). Git is scripted per-op, mirroring the Python
 * twin's ScriptedGit: each op's "git" list is consumed in order.
 */
#include "hermes_json.h"
#include "subagent_worktree.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot read %s\n", path); exit(1); }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)n + 1);
    size_t r = fread(buf, 1, (size_t)n, f); buf[r] = '\0';
    fclose(f);
    return buf;
}

/* Scripted git: ops carry a "git" array of {rc,stdout,stderr}; consume in
 * order. Also records nothing — the twin doesn't diff call logs. */
static json_t *script = NULL;
static size_t script_pos = 0;

static int scripted_git(const char *const args[], const char *cwd,
                        char **out_stdout, char **out_stderr)
{
    (void)args; (void)cwd;
    if (out_stdout) *out_stdout = NULL;
    if (out_stderr) *out_stderr = NULL;
    if (!script || script->type != JSON_ARRAY || script_pos >= json_len(script)) {
        if (out_stdout) *out_stdout = strdup("");
        if (out_stderr) *out_stderr = strdup("script exhausted");
        return -1;
    }
    json_t *r = json_get(script, script_pos++);
    long rc = (long)json_get_num(r, "rc", 0.0);
    const char *so = json_get_str(r, "stdout", "");
    const char *se = json_get_str(r, "stderr", "");
    if (out_stdout) *out_stdout = strdup(so);
    if (out_stderr) *out_stderr = strdup(se);
    return (int)rc;
}

/* The Python twin probes os.path.isdir on the payload path; give both sides
 * the same answer via the fixture's "exists" flag mapped onto real dirs:
 * exists=true -> /tmp (a real dir), false -> /nonexistent_swt_probe. */
static const char *map_probe_path(const char *p, int want_exists)
{
    struct stat st;
    int real = (p && p[0] && stat(p, &st) == 0 && S_ISDIR(st.st_mode));
    if (!p || !p[0]) return p;
    if (real == want_exists) return p;
    return want_exists ? "/tmp" : "/nonexistent_swt_probe";
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <fixture>\n", argv[0]); return 1; }
    subagent_worktree_run_git_hook = scripted_git;
    char *raw = read_file(argv[1]);
    json_t *root = json_parse(raw, NULL);
    if (!root) { fprintf(stderr, "bad fixture\n"); free(raw); return 1; }
    json_t *ops = json_object_get(root, "ops");
    if (ops && ops->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(ops); i++) {
            json_t *c = json_get(ops, i);
            const char *kind = json_get_str(c, "kind", "");
            script = json_object_get(c, "git");
            script_pos = 0;
            json_t *o = json_new_object();
            json_t *out = NULL;

            if (strcmp(kind, "resolve_repo_root") == 0) {
                char *r = resolve_repo_root(json_get_str(c, "path", ""));
                out = r ? json_string(r) : json_new_null();
                free(r);
            } else if (strcmp(kind, "create") == 0) {
                const char *id = json_get_str(c, "id", "");
                /* deterministic id required by both sides */
                char *j = create_subagent_worktree(json_get_str(c, "cwd", ""),
                                                   id[0] ? id : NULL);
                if (!id[0] && j) {
                    /* random-id op: C and Py ids differ — normalize to marker */
                    json_t *parsed = json_parse(j, NULL);
                    if (parsed) { json_object_set(parsed, "id", json_string("<rand>")); }
                    out = parsed ? parsed : json_new_null();
                    free(j);
                } else {
                    out = j ? json_parse(j, NULL) : json_new_null();
                    free(j);
                }
                /* create() calls mkdir on .worktrees under repo root; in the
                 * sandbox that's @SBX@-based and harmless. */
                (void)0;
            } else if (strcmp(kind, "mark_unproven") == 0) {
                json_t *payload = json_parse(json_dumps(json_object_get(c, "payload"), 0), NULL);
                if (!payload) payload = json_new_object();
                const char *u = json_get_str(c, "unmeasured", "\x01__absent__");
                out = mark_worktree_payload_unproven(payload,
                                                     json_get_str(c, "reason", ""),
                                                     strcmp(u, "\x01__absent__") == 0 ? NULL : u);
                (void)out;
                out = payload;
            } else if (strcmp(kind, "unproven_payload") == 0) {
                out = unproven_worktree_payload(json_object_get(c, "info"),
                                                json_get_str(c, "reason", ""));
            } else if (strcmp(kind, "finalize") == 0) {
                json_t *info_in = json_object_get(c, "info");
                json_t *info = json_parse(json_dumps(info_in, 0), NULL);
                int prune = json_object_get(c, "prune")
                            ? (json_bool_value(json_object_get(c, "prune")) ? 1 : 0) : 1;
                int exists = json_object_get(c, "exists")
                             ? (json_bool_value(json_object_get(c, "exists")) ? 1 : 0) : 1;
                json_t *pp = json_object_get(info, "path");
                if (pp && pp->type == JSON_STRING) {
                    const char *mapped = map_probe_path(pp->str_val, exists);
                    if (mapped != pp->str_val)
                        json_object_set(info, "path", json_string(mapped));
                }
                char *j = finalize_subagent_worktree(info, prune);
                out = j ? json_parse(j, NULL) : json_new_null();
                free(j);
            } else if (strcmp(kind, "context_note") == 0) {
                json_t *info = json_object_get(c, "info");
                char *note = build_worktree_context_note(
                    json_get_str(info, "path", ""), json_get_str(info, "branch", ""));
                out = json_string(note ? note : "");
                free(note);
            } else {
                out = json_new_null();
            }

            json_object_set(o, "out", out ? out : json_new_null());
            char *line = json_dumps(o, 0);
            printf("%s\n", line ? line : "{}");
            free(line);
        }
    }
    fflush(stdout);
    return 0;
}
