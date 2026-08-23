#include <stdlib.h>
/* Link shims: yaml bridge + dashboard-events helper are off the
 * normalisation path but referenced by web_server_themes.c. */
void *yaml_parse_file(const char *p) { (void)p; return NULL; }
void *yaml_to_json_string(const char *s) { (void)s; return NULL; }
void yaml_free(void *x) { (void)x; }
const char *ws_events_theme_css_esc(const char *s) { return s; }
