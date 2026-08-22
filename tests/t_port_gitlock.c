/*
 * t_port_gitlock.c — parity harness for src/hermes_cli/gitlock.c
 * (hermes_cli/gitlock.py). Reads a JSON fixture (argv[1]) with "ops" and
 * emits one JSON line per op, mirroring tests/sta_oracle_gitlock.py.
 *
 * Fixture layout (materialized by gen_fixture.sh):
 *   <fixdir>/r1/.git/shallow.lock        (mtime: LOCK_MTIME, old)
 *   <fixdir>/r1/.git/index.lock          (mtime: LOCK_MTIME)
 *   <fixdir>/r2/.git/HEAD.lock           (mtime: NOW_BASE - 599, boundary-fresh)
 *   <fixdir>/r2/.git/MERGE_HEAD.lock     (mtime: LOCK_MTIME)
 *   <fixdir>/rgit/                       (real git repo, 2 commits on main)
 */
#include "gitlock.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot read %s\n", path); exit(1); }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char *buf = malloc(n + 1);
    size_t r = fread(buf, 1, n, f); buf[r] = '\0';
    fclose(f);
    return buf;
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <fixture>\n", argv[0]); return 1; }
    /* The runner feeds a /tmp copy of the fixture; the real fixture dir
     * (with the git repos) is exported as ORACLE_FIXDIR. */
    const char *fixenv = getenv("ORACLE_FIXDIR");
    char fixbuf[4096];
    if (!fixenv || !*fixenv) {
        snprintf(fixbuf, sizeof(fixbuf), "%s", argv[1]);
        fixenv = dirname(fixbuf);
    }
    const char *d = fixenv;
    size_t dlen = strlen(d);

    char *raw = read_file(argv[1]);
    json_t *root = json_parse(raw, NULL);
    if (!root) { fprintf(stderr, "bad fixture\n"); free(raw); return 1; }
    json_t *ops = json_obj_get(root, "ops");
    if (ops && ops->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(ops); i++) {
            json_t *c = json_get(ops, i);
            const char *kind = json_get_str(c, "kind", "");
            json_t *o = json_object();
            if (strcmp(kind, "clear") == 0) {
                const char *repo = json_get_str(c, "repo", "");
                char repo_path[4600];
                snprintf(repo_path, sizeof(repo_path), "%s/%s", d, repo);
                long long min_age =
                    (long long)json_get_num(c, "min_age", -1);
                long long now = (long long)json_get_num(c, "now", 0);
                int running = (int)json_get_num(c, "git_running", 0);
                json_t *removed = gitlock_clear_stale_git_locks_checked(
                    repo_path, min_age, now, running);
                /* emit paths relative to the fixture dir for byte parity */
                json_t *rel = json_array();
                for (size_t k = 0; k < json_len(removed); k++) {
                    json_t *p = json_get(removed, k);
                    const char *ps = p->str_val;
                    if (strncmp(ps, d, dlen) == 0 && ps[dlen] == '/')
                        ps += dlen + 1;
                    json_append(rel, json_string(ps));
                }
                json_free(removed);
                json_object_set(o, "out", rel);
            } else if (strcmp(kind, "ancestor") == 0) {
                const char *repo = json_get_str(c, "repo", "");
                char repo_path[4600];
                snprintf(repo_path, sizeof(repo_path), "%s/%s", d, repo);
                int r = is_ancestor_of_head(
                    repo_path, json_get_str(c, "rev", ""));
                json_object_set(o, "out", json_bool(r ? 1 : 0));
            } else {
                json_object_set(o, "out", json_new_null());
            }
            char *line = json_dumps(o, 0);
            printf("%s\n", line);
            free(line);
            json_free(o);
        }
    }
    json_free(root);
    free(raw);
    return 0;
}
