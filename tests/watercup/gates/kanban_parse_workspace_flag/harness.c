/* gate: kanban_parse_workspace_flag — traced copy of
 * src/hermes_cli/kanban_format.c::kanban_parse_workspace_flag
 * (LIVE twin: hermes_cli/kanban.py::_parse_workspace_flag).
 *
 * Branch ladder (must match twin.py EXACTLY):
 *   B1: empty value                -> ("scratch", None)
 *   V1: stripped value
 *   B2: v in {scratch, worktree}   -> (v, None)
 *   B3/B4: prefix dir: / worktree:
 *     V2: path after prefix (stripped)
 *     B5: path empty -> error
 *     V3: expanded path (expanduser)
 *   B6: unknown -> error
 *   V4: outcome encoding "kind\tpath" or "ERR\t<msg>"
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "watercup.h"

static void emit(wcup_t *_wc, char *kind, char *path, const char *err) {
    char line[768];
    if (err) snprintf(line, sizeof line, "ERR\t%s", err);
    else snprintf(line, sizeof line, "%s\t%s", kind, path ? path : "");
    wcup_valstr(_wc, line);
    char cup[32];
    wcup_finalize(_wc, cup, (int)sizeof cup);
    printf("%s\n", cup);
    free(kind); free(path);
}

static void wc_parse_workspace_flag(const char *value) {
    char *out_kind = NULL, *out_path = NULL;
    WCUP_BEGIN("parse_workspace_flag");
    if (!value || !*value) {                            /* B1 */
        WCUP_BRANCH(1);
        out_kind = strdup("scratch");
        emit(&_wc, out_kind, NULL, NULL);
        WCUP_END("parse_workspace_flag");
        return;
    }
    WCUP_BRANCH(0);
    {
        char v[512];
        snprintf(v, sizeof v, "%s", value);
        char *p = v; while (*p==' '||*p=='\t') p++;
        size_t L = strlen(p); while (L>0 && (p[L-1]==' '||p[L-1]=='\t')) p[--L]='\0';
        WCUP_VALSTR(p);                                 /* V1 */
        int b2 = (strcmp(p,"scratch")==0 || strcmp(p,"worktree")==0);  /* B2 */
        WCUP_BRANCH(b2);
        if (b2) { out_kind = strdup(p); emit(&_wc, out_kind, NULL, NULL); WCUP_END("parse_workspace_flag"); return; }
        struct { const char *pre; const char *kind; } prefixes[] = {
            {"dir:", "dir"}, {"worktree:", "worktree"}
        };
        for (int i = 0; i < 2; i++) {                   /* B3 (dir:) / B4 (worktree:) */
            size_t pl = strlen(prefixes[i].pre);
            int hit = strncmp(p, prefixes[i].pre, pl) == 0;
            WCUP_BRANCH(hit);
            if (!hit) continue;
            char *path = p + pl;
            while (*path==' '||*path=='\t') path++;
            WCUP_VALSTR(path);                          /* V2 */
            if (!*path) {                               /* B5 */
                WCUP_BRANCH(1);
                char msg[128];
                /* NOTE: real port hardcodes "dir:" here even for worktree:
                 * this gate only exercises the agreed dir:/worktree:<path>
                 * non-empty sub-contract plus scratch/worktree/unknown. */
                snprintf(msg, sizeof msg, "--workspace %s requires a path after the colon",
                         prefixes[i].pre);
                emit(&_wc, NULL, NULL, msg);
                WCUP_END("parse_workspace_flag");
                return;
            }
            WCUP_BRANCH(0);
            char exp[600];                              /* expanduser ~ */
            if (path[0]=='~') {
                const char *home = getenv("HOME");
                if (!home) home="";
                size_t hl=strlen(home);
                if (path[1]=='/'||path[1]=='\0')
                    snprintf(exp,sizeof exp,"%s%s",home,path+1);
                else
                    snprintf(exp,sizeof exp,"%s/%s",home,path+1);
            } else {
                snprintf(exp,sizeof exp,"%s",path);
            }
            WCUP_VALSTR(exp);                           /* V3 */
            out_kind = strdup(prefixes[i].kind);
            out_path = strdup(exp);
            emit(&_wc, out_kind, out_path, NULL);
            WCUP_END("parse_workspace_flag");
            return;
        }
        WCUP_BRANCH(1);                                 /* B6 unknown */
        {
            char msg[640];
            snprintf(msg, sizeof(msg),
                "unknown --workspace value '%s': use scratch, worktree, worktree:<path>, or dir:<path>",
                value ? value : "");
            emit(&_wc, NULL, NULL, msg);
        }
    }
    WCUP_END("parse_workspace_flag");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s parse_workspace_flag <value>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) wc_parse_workspace_flag(argv[i]);
    return 0;
}
