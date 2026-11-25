/* Probe: does projects_db_connect + list work outside the tool handler? */
#define SDL_MAIN_HANDLED
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "projects_db.h"

/* Stand-in for the core helper so the probe links on its own. */
void hermes_get_home(char *buf, size_t sz) { snprintf(buf, sz, "%s", "C:/msys64/home/eman5/.slermes"); }

int main(void) {
    char home[1024];
    hermes_get_home(home, sizeof home);
    printf("home=%s\n", home);
    projects_db_t *db = projects_db_connect(home);
    printf("db=%p\n", (void *)db);
    if (!db) return 1;
    char *active = projects_db_get_active_id(db);
    printf("active=%s\n", active ? active : "(none)");
    free(active);
    int n = 0;
    project_t *all = projects_db_list_projects(db, true, &n);
    printf("count=%d arr=%p\n", n, (void *)all);
    for (int i = 0; i < n; i++) printf("  [%d] %s %s\n", i, all[i].id ? all[i].id : "?", all[i].name ? all[i].name : "?");
    if (all) projects_db_free_projects(all, n);
    projects_db_close(db);
    return 0;
}
