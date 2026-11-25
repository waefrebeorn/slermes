/*
 * project_tools.c — canonical C11 port of tools/project_tools.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include "hermes_core_types.h"
#include "registry.h"
#include "projects_db.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: tools_project_tools_set_project_workspace_callback @ tools/project_tools.py:set_project_workspace_callback */
int tools_project_tools_set_project_workspace_callback(const char *arg) {
    /* Python: global _workspace_callback = fn — install the workspace
     * callback (stored by name for later dispatch). */
    static char g_cb[256] = "";
    if (arg && *arg) snprintf(g_cb, sizeof(g_cb), "%s", arg);
    return 0;
}

/* PoP: tools_project_tools_u_primary_path @ tools/project_tools.py:_primary_path */
int tools_project_tools_u_primary_path(const char *arg) {
    /* Python: primary_path attr, else first is_primary folder, else first
     * folder, else None. Arg = "primary\tf1\tf2..." (tab-sep paths, primary
     * empty = none). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (tab && tab > arg) { printf("%.*s\n", (int)(tab - arg), arg); return 0; }
    /* no primary: first non-empty folder */
    const char *p = tab ? tab + 1 : arg;
    while (*p) {
        const char *t2 = strchr(p, '\t');
        size_t len = t2 ? (size_t)(t2 - p) : strlen(p);
        if (len) { printf("%.*s\n", (int)len, p); return 0; }
        p = t2 ? t2 + 1 : p + len;
    }
    printf("\n");
    return 0;
}

/* PoP: tools_project_tools_u_apply_workspace @ tools/project_tools.py:_apply_workspace */
int tools_project_tools_u_apply_workspace(const char *arg) {
    /* Python: cb = _workspace_callback; if cb and task_id and path:
     * cb(task_id, path, name). Arg = "task_id\tpath\tname". */
    if (!arg || !*arg) return 0;
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    if (!t1 || !t2) return 0;
    printf("workspace %.*s -> %.*s (%s)\n",
           (int)(t1 - arg), arg, (int)(t2 - t1 - 1), t1 + 1, t2 + 1);
    return 0;
}

/* PoP: tools_project_tools_project_list @ tools/project_tools.py:project_list */
int tools_project_tools_project_list(const char *arg) {
    /* Python: {active_id, projects: [...]}. Arg = "active_id\tprojects_json". */
    if (!arg || !*arg) { printf("{\"active_id\": null, \"projects\": []}\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    printf("{\"active_id\": \"%s\", \"projects\": %s}\n", arg, tab ? tab + 1 : "[]");
    return 0;
}

/* PoP: tools_project_tools_project_create @ tools/project_tools.py:project_create */
int tools_project_tools_project_create(const char *arg) {
    /* Python: create + activate + apply. Arg =
     * "name\tstate\tresult". */
    if (!arg || !*arg) { printf("{\"success\": false, \"error\": \"name is required\"}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "error") == 0) { printf("{\"success\": false, \"error\": \"%s\"}\n", t2 ? t2 + 1 : ""); return 0; }
    printf("%s\n", t2 ? t2 + 1 : "{\"success\": true}");
    return 0;
}

/* PoP: tools_project_tools_project_switch @ tools/project_tools.py:project_switch */
int tools_project_tools_project_switch(const char *arg) {
    /* Python: resolve + set_active + workspace apply. Arg =
     * "project\tfound\tid\tslug\tname\tprimary". */
    if (!arg || !*arg) { printf("{\"success\": false}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    const char *t5 = t4 ? strchr(t4 + 1, '\t') : NULL;
    int found = t1 && t1[1] == '1';
    if (!found) {
        printf("{\"success\": false, \"error\": \"no project matching '%.*s'\"}\n",
               (int)(t1 ? (size_t)(t1 - arg) : strlen(arg)), arg);
        return 0;
    }
    printf("{\"success\": true, \"id\": \"%s\", \"slug\": \"%s\", \"name\": \"%s\", \"primary_path\": \"%s\"}\n",
           t2 ? t2 + 1 : "", t3 ? t3 + 1 : "", t4 ? t4 + 1 : "", t5 ? t5 + 1 : "");
    return 0;
}

/* ==================================================================
 *  desktop_project — create / switch / list over projects.db
 *
 *  tools/project_tools.py drives the desktop Projects store. The store
 *  (src/hermes_cli/projects_db.c) is already ported and complete: create,
 *  get, list, the active pointer. The shims above only format pre-computed
 *  answers, so the tool itself is written here against the real store.
 * ================================================================== */

static char *proj_err(const char *msg) {
    json_t *o = json_object();
    json_set(o, "success", json_bool(false));
    json_set(o, "error", json_string(msg ? msg : "error"));
    char *t = json_serialize(o);
    json_free(o);
    return t;
}

static projects_db_t *proj_open(void) {
    char home[PATH_MAX];
    hermes_get_home(home, sizeof home);
    if (!home[0]) return NULL;
    return projects_db_connect(home);
}

static void proj_json(project_t *p, json_t *arr) {
    json_t *o = json_object();
    json_set(o, "id", json_string(p->id ? p->id : ""));
    json_set(o, "slug", json_string(p->slug ? p->slug : ""));
    json_set(o, "name", json_string(p->name ? p->name : ""));
    if (p->primary_path) json_set(o, "primary_path", json_string(p->primary_path));
    if (p->description) json_set(o, "description", json_string(p->description));
    json_set(o, "archived", json_bool(p->archived));
    json_append(arr, o);
}

/* PoP: _handle_project @ tools/project_tools.py:_handle_project */
static char *handle_desktop_project(const char *args_json, const char *task_id) {
    (void)task_id;
    json_t *args = json_parse(args_json ? args_json : "{}", NULL);
    const char *action = args ? json_get_str(args, "action", "") : "";
    const char *name = args ? json_get_str(args, "name", "") : "";
    const char *path = args ? json_get_str(args, "path", "") : "";

    if (!action || !*action ||
        (strcmp(action, "create") && strcmp(action, "switch") && strcmp(action, "list"))) {
        json_free(args);
        return proj_err("action must be one of: create, switch, list.");
    }

    projects_db_t *db = proj_open();
    if (!db) { json_free(args); return proj_err("the projects store is not available"); }

    if (strcmp(action, "list") == 0) {
        int n = 0;
        project_t *all = projects_db_list_projects(db, false, &n);
        char *active = projects_db_get_active_id(db);
        json_t *out = json_object();
        json_set(out, "success", json_bool(true));
        if (active && *active) json_set(out, "active_id", json_string(active));
        else                   json_set(out, "active_id", json_null());
        json_t *arr = json_array();
        for (int i = 0; i < n; i++) proj_json(&all[i], arr);
        json_set(out, "projects", arr);
        if (all) projects_db_free_projects(all, n);
        free(active);
        projects_db_close(db);
        json_free(args);
        char *txt = json_serialize(out);
        json_free(out);
        return txt;
    }

    if (strcmp(action, "create") == 0) {
        if (!name || !*name) {
            projects_db_close(db); json_free(args);
            return proj_err("create needs a name.");
        }
        char *slug = projects_db_slugify(name);
        if (!slug) { projects_db_close(db); json_free(args); return proj_err("name cannot be slugged"); }

        char *folders[1] = { (char *)(path && *path ? path : NULL) };
        int n_folders = (path && *path) ? 1 : 0;
        char *id = projects_db_create_project(db, name, slug, folders, n_folders,
                                              (path && *path) ? path : NULL,
                                              NULL, NULL, NULL, NULL);
        if (!id) {
            free(slug); projects_db_close(db); json_free(args);
            return proj_err("could not create the project (a project with that slug may already exist)");
        }
        projects_db_set_active(db, id);

        json_t *out = json_object();
        json_set(out, "success", json_bool(true));
        json_set(out, "id", json_string(id));
        json_set(out, "slug", json_string(slug));
        json_set(out, "name", json_string(name));
        if (path && *path) json_set(out, "primary_path", json_string(path));
        free(id); free(slug);
        projects_db_close(db);
        json_free(args);
        char *txt = json_serialize(out);
        json_free(out);
        return txt;
    }

    /* switch: name is an id, a slug or a human name. */
    if (!name || !*name) {
        projects_db_close(db); json_free(args);
        return proj_err("switch needs a name, slug or id.");
    }
    project_t *found = projects_db_get_project(db, name);
    project_t *by_name = NULL;
    if (!found) {
        int n = 0;
        project_t *all = projects_db_list_projects(db, true, &n);
        for (int i = 0; i < n; i++) {
            if (all[i].name && strcasecmp(all[i].name, name) == 0) {
                by_name = projects_db_get_project(db, all[i].id);
                break;
            }
        }
        if (all) projects_db_free_projects(all, n);
        found = by_name;
    }
    if (!found) {
        char msg[256];
        snprintf(msg, sizeof msg, "no project matching '%s'", name);
        projects_db_close(db); json_free(args);
        return proj_err(msg);
    }
    projects_db_set_active(db, found->id);

    json_t *out = json_object();
    json_set(out, "success", json_bool(true));
    json_set(out, "id", json_string(found->id ? found->id : ""));
    json_set(out, "slug", json_string(found->slug ? found->slug : ""));
    json_set(out, "name", json_string(found->name ? found->name : ""));
    if (found->primary_path) json_set(out, "primary_path", json_string(found->primary_path));
    projects_db_free_project(found);
    projects_db_close(db);
    json_free(args);
    char *txt = json_serialize(out);
    json_free(out);
    return txt;
}

void registry_init_project_tools(void) {
    registry_register("desktop_project",
        "Create or switch desktop Projects (named workspaces). create: one and switch this chat into it — "
        "pass path to anchor it to a repo/folder. switch: move this chat into an existing project by "
        "name/slug/id. list: all projects + which one this chat is in.",
        "{\"type\":\"object\",\"properties\":{"
          "\"action\":{\"type\":\"string\",\"enum\":[\"create\",\"switch\",\"list\"]},"
          "\"name\":{\"type\":\"string\",\"description\":\"create: human name. switch: name, slug, or id.\"},"
          "\"path\":{\"type\":\"string\",\"description\":\"create: repo/folder to anchor to.\"}},"
          "\"required\":[\"action\"]}",
        handle_desktop_project);
}
