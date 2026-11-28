/* gate: port__gateway_shutdown_forensics_py___proc_summary
 * forensics_proc_summary is `static` (cross-TU extern can't reach it). Inline
 * its body + its two /proc helpers verbatim, serialize the JSON node. On
 * Windows /proc is absent so all /proc reads return NULL and the summary is
 * just {"pid": N} -- the twin's _proc_summary reads /proc too, so it also
 * yields {"pid": N}. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <signal.h>
#include "hermes_json.h"
#include "watercup.h"

static char *local_read_proc_field(pid_t pid, const char *key) {
    char path[128];
    snprintf(path, sizeof(path), "/proc/%d/status", (int)pid);
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    char line[256];
    char *result = NULL;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, key, strlen(key)) == 0 && line[strlen(key)] == ':') {
            const char *val = line + strlen(key) + 1;
            while (*val == ' ' || *val == '\t') val++;
            size_t len = strlen(val);
            while (len > 0 && (val[len-1] == '\n' || val[len-1] == '\r')) len--;
            result = malloc(len + 1);
            if (result) { memcpy(result, val, len); result[len] = '\0'; }
            break;
        }
    }
    fclose(f);
    return result;
}

static char *local_read_proc_cmdline(pid_t pid) {
    char path[128];
    snprintf(path, sizeof(path), "/proc/%d/cmdline", (int)pid);
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    char raw[4096];
    size_t n = fread(raw, 1, sizeof(raw) - 1, f);
    fclose(f);
    if (n == 0) return NULL;
    raw[n] = '\0';
    for (size_t i = 0; i < n; i++) if (raw[i] == '\0') raw[i] = ' ';
    char *result = strdup(raw);
    if (result) {
        size_t len = strlen(result);
        while (len > 0 && result[len-1] == ' ') result[--len] = '\0';
    }
    return result;
}

static json_node_t *local_proc_summary(pid_t pid) {
    json_node_t *summary = json_new_object();
    if (!summary) return NULL;
    json_object_set(summary, "pid", json_new_number(pid));
    if (pid <= 0) return summary;
    char *name = local_read_proc_field(pid, "Name");
    if (name) { json_object_set(summary, "name", json_new_string(name)); free(name); }
    char *state = local_read_proc_field(pid, "State");
    if (state) { json_object_set(summary, "state", json_new_string(state)); free(state); }
    char *ppid_str = local_read_proc_field(pid, "PPid");
    if (ppid_str) { json_object_set(summary, "ppid", json_new_number(atol(ppid_str))); free(ppid_str); }
    char *uid = local_read_proc_field(pid, "Uid");
    if (uid) {
        char *space = strchr(uid, ' ');
        if (space) *space = '\0';
        json_object_set(summary, "uid", json_new_string(uid));
        free(uid);
    }
    char *cmdline = local_read_proc_cmdline(pid);
    if (cmdline) {
        size_t len = strlen(cmdline);
        if (len > 300) cmdline[300] = '\0';
        json_object_set(summary, "cmdline", json_new_string(cmdline));
        free(cmdline);
    }
    return summary;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s forensics_proc_summary <pid>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        WCUP_BEGIN("forensics_proc_summary");
        fflush(stdout);
        json_node_t * result = local_proc_summary((int)strtol(argv[i], NULL, 10));
        char *js = result ? json_serialize(result) : NULL;
        char buf[512];
        snprintf(buf, sizeof(buf), "%.500s", js ? js : "");
        if (js) free(js);
        if (result) json_free(result);
        fflush(stdout);
        WCUP_VALSTR(buf);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("forensics_proc_summary");
    }
    return 0;
}
