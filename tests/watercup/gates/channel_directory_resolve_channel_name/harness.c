/* gate: channel_directory_resolve_channel_name — drives the REAL
 * src/hermes_cli/channel_directory.c::resolve_channel_name (linked via
 * extra_src; credfiles_get_hermes_home shimmed to a fixture dir that main()
 * writes before tracing).
 * LIVE twin: gateway/channel_directory.py::resolve_channel_name.
 *
 * Input: "<platform>|<query>".
 * Rung order (must match twin.py EXACTLY):
 *   B1  resolution succeeded (non-NULL id)
 *   V   resolved id, or "" when unresolved
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "watercup.h"

/* Shim: point the port's directory loader at our deterministic fixture. */
const char *credfiles_get_hermes_home(void) { return "/tmp/wc_channel_dir_fixture"; }

static const char *FIXTURE_JSON =
"{"
"\"updated_at\":null,"
"\"platforms\":{"
"\"discord\":["
"{\"id\":\"123\",\"name\":\"bot-home\",\"guild\":\"Wubu HQ\"},"
"{\"id\":\"456\",\"name\":\"general\",\"guild\":\"Wubu HQ\"},"
"{\"id\":\"789\",\"name\":\"projects\",\"guild\":\"Side Guild\"},"
"{\"id\":\"999\",\"name\":\"general-announcements\",\"guild\":\"Side Guild\"}"
"],"
"\"telegram\":["
"{\"id\":\"-100200\",\"name\":\"Ops Room\",\"type\":\"group\"},"
"{\"id\":\"-100300\",\"name\":\"alerts\",\"type\":\"channel\"}"
"],"
"\"slack\":["
"{\"id\":\"C0B0QV5434G\",\"name\":\"engineering\"},"
"{\"id\":\"C111\",\"name\":\"random\"}"
"]}}";

char *resolve_channel_name(const char *platform_name, const char *name);

static void write_fixture(void) {
    mkdir("/tmp/wc_channel_dir_fixture", 0755);
    FILE *f = fopen("/tmp/wc_channel_dir_fixture/channel_directory.json", "w");
    if (!f) return;
    fputs(FIXTURE_JSON, f);
    fclose(f);
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s resolve_channel_name <platform|query>...\n", argv[0]); return 2; }
    write_fixture();
    for (int i = 2; i < argc; i++) {
        char buf[1024];
        snprintf(buf, sizeof buf, "%s", argv[i]);
        char *pipe = strchr(buf, '|');
        if (pipe) *pipe = '\0';
        const char *platform = buf;
        const char *query = pipe ? pipe + 1 : "";

        WCUP_BEGIN("resolve_channel_name");
        char *id = resolve_channel_name(platform, query);
        WCUP_BRANCH(id != NULL);                       /* B1 resolved */
        char val[1024];
        snprintf(val, sizeof val, "%s", id ? id : "");
        WCUP_VALSTR(val);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("resolve_channel_name");
        free(id);
    }
    return 0;
}
