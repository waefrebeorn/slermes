/* t_port_plugin_commands_gap.c — exercises the public get_plugin_commands facade
 * against the revived registry state. Seams only satisfy discovery; the command
 * table itself comes from py_plugin_install_state.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../lib/libplugin/plugin.c"

static json_t *g_io;
static json_t *gap_seam(void *ud, const char *name, const json_t *args) {
    (void)ud; (void)args;
    json_t *v = g_io ? json_obj_get(g_io, name) : NULL;
    return v ? json_copy(v) : NULL;
}
static void emit(json_t *v) { char *s = json_dumps(v, 0); fputs(s ? s : "null", stdout); free(s); }

int main(void) {
    char line[32768];
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *c = json_parse(line, NULL);
        if (!c) continue;
        g_io = json_obj_get(c, "io");
        py_plugin_reset();
        py_plugin_set_seam(gap_seam, NULL);
        py_plugin_install_state(json_obj_get(c, "state"));
        json_t *out = plugin_get_commands();
        fputs("{\"state\":", stdout); emit(json_obj_get(c, "state"));
        fputs(",\"out\":", stdout); emit(out);
        fputs("}\n", stdout);
        json_free(out); json_free(c);
    }
    return 0;
}
