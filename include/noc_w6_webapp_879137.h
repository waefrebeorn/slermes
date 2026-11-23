#ifndef NOC_W6_WEBAPP_879137_H
#define NOC_W6_WEBAPP_879137_H
#include <stddef.h>
/* Resolved render-bundle directory: packaged env value when set, else
 * <project_root>/apps/desktop/dist-webapp (forward-slash form). */
/* PoP: w6_webapp_879137_dist_dir @ hermes_cli/webapp.py:webapp_dist_dir */
char *w6_webapp_879137_dist_dir(const char *project_root, const char *dist_env);
/* False for a packaged dist or when no index.html is built; True otherwise. */
/* PoP: w6_webapp_879137_mark_stale @ hermes_cli/webapp.py:mark_webapp_stale */
int w6_webapp_879137_mark_stale(int packaged, int index_exists);
/* PoP: w6_webapp_879137_bundle_stale @ hermes_cli/webapp.py:webapp_bundle_stale */
int w6_webapp_879137_bundle_stale(int marker_exists);
void w6_webapp_879137_free_str(char *s);
#endif
