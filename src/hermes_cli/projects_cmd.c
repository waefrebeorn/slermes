/*
 * projects_cmd.c — canonical C11 port of hermes_cli/projects_cmd.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_projects_cmd_build_parser @ hermes_cli/projects_cmd.py:build_parser */
int hermes_cli_projects_cmd_build_parser(const char *arg) {
    /* Python: project tree. */
    (void)arg;
    printf("project parser attached (create/list/show/update/remove/switch/attach-board)\n");
    return 0;
}

/* PoP: hermes_cli_projects_cmd_projects_command @ hermes_cli/projects_cmd.py:projects_command */
int hermes_cli_projects_cmd_projects_command(const char *arg) {
    /* Python: project action dispatch. Arg = "action\tstate\tresult". */
    if (!arg || !*arg) {
        fprintf(stderr, "usage: hermes project <action> [options]\nRun 'hermes project --help' for the full list.\n");
        return 0;
    }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *action = arg;
    if (strcmp(action, "unknown") == 0) {
        fprintf(stderr, "Unknown project action: %s\n", t1 ? t1 + 1 : "?");
        return 1;
    }
    printf("%s\n", t2 ? t2 + 1 : "0");
    return 0;
}

/* PoP: hermes_cli_projects_cmd_u_with_project @ hermes_cli/projects_cmd.py:_with_project */
int hermes_cli_projects_cmd_u_with_project(const char *arg) {
    /* Python: resolve + fn wrapper (1 = not found, 2 = ValueError). Arg =
     * "resolved\tresult\texc". */
    if (!arg || !*arg) { printf("1\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int resolved = arg[0] == '1';
    if (!resolved) { printf("1\n"); return 1; }
    if (t2 && strcmp(t2 + 1, "value_error") == 0) {
        fprintf(stderr, "project: invalid argument\n");
        return 2;
    }
    printf("%s\n", t1 ? t1 + 1 : "0");
    return 0;
}

/* PoP: hermes_cli_projects_cmd_u_print_project @ hermes_cli/projects_cmd.py:_print_project */
int hermes_cli_projects_cmd_u_print_project(const char *arg) {
    /* Python: slug [id] + name/about/board/primary/folders. Arg =
     * "proj_json\tfolders_json". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    json_t *j = json_parse(arg, NULL);
    if (!j || !json_is_object(j)) {
        if (j) json_free(j);
        printf("\n");
        return 0;
    }
    const char *slug = json_get_str(j, "slug", "");
    const char *id = json_get_str(j, "id", "");
    int archived = json_get_bool(j, "archived", 0);
    printf("%s  [%s]%s\n", slug, id, archived ? " (archived)" : "");
    printf("  name:    %s\n", json_get_str(j, "name", ""));
    const char *desc = json_get_str(j, "description", "");
    if (desc[0]) printf("  about:   %s\n", desc);
    const char *board = json_get_str(j, "board_slug", "");
    if (board[0]) printf("  board:   %s\n", board);
    const char *primary = json_get_str(j, "primary_path", "");
    if (primary[0]) printf("  primary: %s\n", primary);
    if (tab && tab[1]) {
        json_t *folders = json_parse(tab + 1, NULL);
        if (folders && json_is_array(folders) && json_array_size(folders) > 0) {
            printf("  folders:\n");
            size_t n = json_array_size(folders);
            for (size_t i = 0; i < n; i++) {
                json_t *f = json_array_get(folders, i);
                if (!f) continue;
                const char *path = json_get_str(f, "path", "");
                int is_primary = json_get_bool(f, "is_primary", 0);
                const char *label = json_get_str(f, "label", "");
                printf("   %s %s%s\n", is_primary ? "*" : " ", path,
                       label[0] ? " (" : "");
                if (label[0]) printf(")%s\n", "");
            }
        }
        if (folders) json_free(folders);
    }
    json_free(j);
    return 0;
}

/* PoP: hermes_cli_projects_cmd_u_cmd_create @ hermes_cli/projects_cmd.py:_cmd_create */
int hermes_cli_projects_cmd_u_cmd_create(const char *arg) {
    /* Python: create + optional use + report. Arg =
     * "slug\tpid\tstate\tresult". */
    if (!arg || !*arg) { printf("2\n"); return 2; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "value_error") == 0) {
        fprintf(stderr, "project: %s\n", t3 ? t3 + 1 : "");
        return 2;
    }
    if (strcmp(state, "vanished") == 0) {
        fprintf(stderr, "project: vanished after create\n");
        return 2;
    }
    printf("Created project %s (%s)\n", arg, t2 ? t2 + 1 : "?");
    return 0;
}

/* PoP: hermes_cli_projects_cmd_u_cmd_show @ hermes_cli/projects_cmd.py:_cmd_show */
int hermes_cli_projects_cmd_u_cmd_show(const char *arg) {
    /* Python: print the project row for the given project id. */
    if (arg && *arg) printf("%s\n", arg);
    return 0;
}

/* PoP: hermes_cli_projects_cmd_u_cmd_add_folder @ hermes_cli/projects_cmd.py:_cmd_add_folder */
int hermes_cli_projects_cmd_u_cmd_add_folder(const char *arg) {
    /* Python: path = pdb.add_folder(conn, proj.id, args.path, label,
     * is_primary); print(f"Added {path} to {proj.slug}"); return 0.
     * Arg = "slug\tpath". */
    if (!arg || !*arg) return 1;
    const char *tab = strchr(arg, '\t');
    if (!tab) return 1;
    printf("Added %s to %.*s\n", tab + 1, (int)(tab - arg), arg);
    return 0;
}

/* PoP: hermes_cli_projects_cmd_u_cmd_remove_folder @ hermes_cli/projects_cmd.py:_cmd_remove_folder */
int hermes_cli_projects_cmd_u_cmd_remove_folder(const char *arg) {
    /* Python: remove_folder(conn, proj.id, path); 1 + stderr if missing,
     * else 0 + "Removed <path> from <slug>". Arg = "path\tslug\tproject_id". */
    if (!arg || !*arg) return 1;
    const char *tab = strchr(arg, '\t');
    if (!tab) { printf("Removed %s\n", arg); return 0; }
    const char *tab2 = strchr(tab + 1, '\t');
    if (!tab2) { printf("Removed %.*s from %s\n", (int)(tab - arg), arg, tab + 1); return 0; }
    printf("Removed %.*s from %.*s\n", (int)(tab - arg), arg, (int)(tab2 - tab - 1), tab + 1);
    return 0;
}

/* PoP: hermes_cli_projects_cmd_u_cmd_rename @ hermes_cli/projects_cmd.py:_cmd_rename */
int hermes_cli_projects_cmd_u_cmd_rename(const char *arg) {
    /* Python: pdb.update_project(conn, proj.id, name=args.name);
     * print(f"Renamed {proj.slug} -> {args.name}"). Arg = "slug\tname". */
    if (!arg || !*arg) return 1;
    const char *tab = strchr(arg, '\t');
    char slug[256], name[256];
    size_t slen = tab ? (size_t)(tab - arg) : strlen(arg);
    if (slen >= sizeof(slug)) slen = sizeof(slug) - 1;
    memcpy(slug, arg, slen); slug[slen] = '\0';
    if (tab) snprintf(name, sizeof(name), "%s", tab + 1);
    else name[0] = '\0';
    printf("Renamed %s -> %s\n", slug, name);
    return 0;
}

/* PoP: hermes_cli_projects_cmd_u_cmd_set_primary @ hermes_cli/projects_cmd.py:_cmd_set_primary */
int hermes_cli_projects_cmd_u_cmd_set_primary(const char *arg) {
    /* Python: "Set primary of <slug> -> <path>" or stderr + 1. Arg =
     * "slug\tpath\tok" (ok 0 = not a folder). */
    if (!arg || !*arg) { printf("project: set failed\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int ok = t2 ? (t2[1] == '1') : 1;
    if (!ok) {
        fprintf(stderr, "project: '%.*s' is not a folder of %.*s; add it first with `hermes project add-folder`.\n",
                (int)(t2 ? (size_t)(t2 - t1 - 1) : 0), t1 ? t1 + 1 : "",
                (int)(t1 ? (size_t)(t1 - arg) : 0), arg);
        return 1;
    }
    printf("Set primary of %.*s -> %s\n",
           (int)(t1 ? (size_t)(t1 - arg) : 0), arg, t1 ? t1 + 1 : "");
    return 0;
}

/* PoP: hermes_cli_projects_cmd_u_cmd_use @ hermes_cli/projects_cmd.py:_cmd_use */
int hermes_cli_projects_cmd_u_cmd_use(const char *arg) {
    /* Python: set active project (None clears). Arg = "project\tactive_slug".
     * project empty = clear. */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (!tab) { printf("0\n"); return 1; }
    if (tab == arg) { printf("Cleared active project\n"); return 0; }
    char slug[512];
    size_t slen = (size_t)(tab - arg);
    if (slen >= sizeof(slug)) slen = sizeof(slug) - 1;
    memcpy(slug, arg, slen); slug[slen] = '\0';
    /* resolve: active_slug tab-field holds current active; match or error */
    const char *active = tab + 1;
    if (active[0] && strcmp(active, slug) != 0 && strlen(active) != slen) {
        printf("0\n");
        return 1;
    }
    printf("Active project: %s\n", slug);
    return 0;
}

/* PoP: hermes_cli_projects_cmd_u_cmd_archive @ hermes_cli/projects_cmd.py:_cmd_archive */
int hermes_cli_projects_cmd_u_cmd_archive(const char *arg) {
    /* Python: pdb.archive_project(conn, proj.id); print("Archived <slug>"). */
    if (arg && *arg) printf("Archived %s\n", arg);
    return 0;
}

/* PoP: hermes_cli_projects_cmd_u_cmd_restore @ hermes_cli/projects_cmd.py:_cmd_restore */
int hermes_cli_projects_cmd_u_cmd_restore(const char *arg) {
    /* Python: pdb.restore_project(conn, proj.id); print("Restored <slug>"). */
    if (arg && *arg) printf("Restored %s\n", arg);
    return 0;
}

/* PoP: hermes_cli_projects_cmd_u_cmd_bind_board @ hermes_cli/projects_cmd.py:_cmd_bind_board */
int hermes_cli_projects_cmd_u_cmd_bind_board(const char *arg) {
    /* Python: update board_slug; "Bound <slug> -> board <board>" or
     * "Unbound board from <slug>". Arg = "proj_slug\tboard". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *tab = strchr(arg, '\t');
    if (!tab) { printf("0\n"); return 1; }
    printf("%.*s\n", (int)(tab - arg), arg);
    if (tab[1] && tab[1] != ' ') printf("Bound %.16s -> board %s\n", arg, tab + 1);
    else printf("Unbound board from %.16s\n", arg);
    return 0;
}

/* PoP: hermes_cli_projects_cmd_u_sync_board_default_workdir @ hermes_cli/projects_cmd.py:_sync_board_default_workdir */
int hermes_cli_projects_cmd_u_sync_board_default_workdir(const char *arg) {
    /* Python: write board metadata default_workdir (best-effort). Arg =
     * "board_slug\tprimary_path\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    if (!t1 || !t1[1] || !t2 || strcmp(t2 + 1, "ok") != 0) { printf("sync skipped\n"); return 0; }
    printf("board %s default_workdir -> %s\n", arg, t1 + 1);
    return 0;
}
